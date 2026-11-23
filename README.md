Assets will hold all asset data like textures and sprites and the font in the case we add that.

fixmath is the 16.16 fixed math float alternative because the board is bad at floats and we need performance.

hardware will hold all the constant pointers to the memory that we read to access like switches, 7-segment display, 8bit pixel buffer and so on.

main will hold as little as possible but probably at least something like a state machine for if we are in the menu, in game or have died. 

player holds the player data like position, score, health if that exists and so on.

renderer will be responsible for rendering which will use most of the frame time so this needs to be fast

world is the world generator and theme picker, so the one that generates the world and chooses what textures and colors it should use. 
