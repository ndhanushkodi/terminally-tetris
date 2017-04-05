#include "controller.h"

int is_user_input() {
    struct termios old_term, new_term;
    int old_fd;
    int user_input;

    // Modify stdin with saved copies of original state
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO); // Enable canonical mode, echo input
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    old_fd = fcntl(STDIN_FILENO, F_GETFL, 0); // Copy stdin fd
    fcntl(STDIN_FILENO, F_SETFL, old_fd | O_NONBLOCK); // Non-blocking

    // Grab user input
    user_input = getchar();

    // Revert stdin to original state
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    fcntl(STDIN_FILENO, F_SETFL, old_fd);

    // If user input is valid
    if(user_input != EOF) {
        // Push user input back to stdin to be read again later
        ungetc(user_input, stdin);
        return 1;
    }

    return 0;
}

void act_on_user_input(
    char user_input, 
    Movement* m,
    int* frame_counter) {

    int frames_until_next_move;
    switch(user_input) {
        case DOWN_KEY:
            m->y = 1; // Move down by 1
            *frame_counter = 0; // Reset counter for default downwards move
            break;
        case LEFT_KEY:
            m->x += -1; // Move left by 1
            break;
        case RIGHT_KEY:
            m->x += 1; // Move right by 1
            break;
        case ROTATE_CW_KEY: // rotate block clockwise
            if (m->r == NO_ROTATE) {
                m->r = RIGHT;
            }
            if (m->r == LEFT) {
                m->r = NO_ROTATE;
            }
            break;
        case ROTATE_CCW_KEY: // rotate block counter-clockwise
            if (m->r == NO_ROTATE) {
                m->r = LEFT;
            }
            if (m->r == RIGHT) {
                m->r = NO_ROTATE;
            }
            break;
        case PAUSE_KEY: // pause the game
            // TODO: @skelly pause the game
            break;
        case QUIT_KEY: // quit the game
            // TODO: @skelly quit the game
            break;
        case BOSS_MODE_KEY: // set the game to boss mode
            // TODO: @skelly set the game to boss mode
            break;
    }
}

int move_block(State* s, Movement* m) {
    // Clear cells occupied by the block
    for (int i = 0; i < 4; i++) {
        int x = s->block->cells[i][0] + s->block->x;
        int y = s->block->cells[i][1] + s->block->y;

        s->grid[x][y] = Empty;
    }

    // Flags
    int applied_move = 1;
    int can_move_vert = 1;
    int can_move_horiz = 1;
    int can_rotate = 1;

    // Keep attempty to apply moves until no moves can be applied
    while (applied_move) { 
        applied_move = 0;

        // Try to move vertically
        if (m->y != 0) {
            can_move_vert = 1;
            for (int i = 0; i < 4; i++) {
                int x = s->block->cells[i][0] + s->block->x;
                int y = s->block->cells[i][1] + s->block->y;

                // Conditions where move is invalid
                if (!in_grid(x, y + m->y) || s->grid[x][y + m->y] != Empty) {
                    can_move_vert = 0;
                    break;
                }
            }
            if (can_move_vert) {
                s->block->y += m->y;
                m->y = 0; // signal that we have performed this operation
                applied_move = 1; // signal that A operation was performed
            }
        }

        // Try to move horizontally
        if (m->x != 0) {
            can_move_horiz = 1;
            for (int i = 0; i < 4; i++) {
                int x = s->block->cells[i][0] + s->block->x;
                int y = s->block->cells[i][1] + s->block->y;

                // Conditions where move is invalid
                if (!in_grid(x + m->x, y) || s->grid[x + m->x][y] != Empty) {
                    can_move_horiz = 0;
                    break;
                }
            }
            if (can_move_horiz) {
                s->block->x += m->x;
                m->x = 0;
                applied_move = 1;
            }
        }

        // Try to rotate
        if (m->r != NO_ROTATE) {
            can_rotate = 1;
            switch (m->r) {
                case LEFT: rotate_left(s->block); break;
                case RIGHT: rotate_right(s->block); break;
            }

            for (int i = 0; i < 4; i++) {
                int x = s->block->cells[i][0] + s->block->x;
                int y = s->block->cells[i][1] + s->block->y;

                // Conditions where move is invalid
                if (!in_grid(x, y) || s->grid[x][y] != Empty) {
                    can_rotate = 0;
                    break;
                }
            }

            if (!can_rotate) {
                // Undo rotation
                switch (m->r) {
                    case LEFT: rotate_right(s->block); break;
                    case RIGHT: rotate_left(s->block); break;
                }
            } else {
                m->r = NO_ROTATE;
                applied_move = 1;
            }
        }
    }

    // Set cells occupied by block to correct color
    for (int i = 0; i < 4; i++) {
        int x = s->block->cells[i][0] + s->block->x;
        int y = s->block->cells[i][1] + s->block->y;

        s->grid[x][y] = s->block->color;
    }

    return can_move_vert;
}

void aggregate_movement(Movement* m, State* s, int* frame_counter) {
    // Initialize with zero movement
    m->x = 0;
    m->y = 0;
    m->r = NO_ROTATE;

    // Default downwards movement
    if (*frame_counter > s->speed) {
        m->y = 1;
        *frame_counter = 0;
    }

    // Get user input
    if (is_user_input()) {
        // The user has pressed a key, take appropriate action
        char user_input = getchar();
        act_on_user_input(user_input, m, frame_counter);
    }
}

void begin_game(State* s) {
    s->block = malloc(sizeof(Block));
    s->next = rand() % NUM_BLOCKS;
    s->mode = RUNNING;
    s->speed = 48;
    spawn(s);

    int frame_counter = 0;
    Movement* net_move = malloc(sizeof(Movement));

    while (s->mode == RUNNING) {
        frame_counter++;
        // Collect desired total movement in this frame
        aggregate_movement(net_move, s, &frame_counter);

        // Perform movement
        if (!move_block(s, net_move)) {
            // Block was unable to make any valid downwards move.

            // Perform row clear if needed and update score
            update_score(s);

            // Spawn a new block
            spawn(s);
        }

        // Rendering loop
        clear();
        render(s);
        refresh();

        // Assuming execution of loop takes negligible time
        usleep(US_DELAY);
    }

    // Shutdown procedure
    clear();
    refresh();
    // TODO: @dbishop terminal state not resetting properly, input not visible
}