#include <conio.h>   // for _getch()
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/player.h"

void input_handle()
{
    char c = _getch();

    switch(c)
    {
        case 'w': player_move(&player, 0, -1); break;
        case 's': player_move(&player, 0, 1); break;
        case 'a': player_move(&player, -1, 0); break;
        case 'd': player_move(&player, 1, 0); break;
    }
}