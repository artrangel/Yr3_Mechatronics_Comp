#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rs232.h"
#include "serial.h"

//Defining the limits for the movements, buffer, and the font data
#define MAX_FONT_DATA 128
#define MAX_MOVEMENTS 100
#define BUFFER_SIZE 100

typedef struct {
    int x;
    int y;
    int pen;
} Movement;

typedef struct {
    int ascii;
    int num_movements;
    Movement movements[MAX_MOVEMENTS];
} FontData;

// Global variables
FontData font[MAX_FONT_DATA];


// Function declarations
void LoadFontData(const char *filename);
void GenerateGCode(char *text, float height, char *buffer);
void SendCommands (char *buffer );


int main() 
{
    //Initialising the required variables to run the software
    char font_file[] = "SingleStrokeFont.txt";
    char text_file[100];
    char text[1000];
    float height;
    char buffer[100];

    // Loading the font data
    printf("Loading font data...\n");
    LoadFontData(font_file);

    // Get the user input for text file and desired height
    printf("Enter text file name: ");
    scanf("%s", text_file);
    printf("Enter height (4-10mm): ");
    scanf("%f", &height);

    //A while loop to repeatedly ask the user to input a height within the range of 4mm-10mm
    while (height < 4 || height > 10) {
        printf("Invalid height. Please use values between 4 and 10mm.\n Enter height again (4-10mm):");
        scanf("%f", &height);
    }

    // Reading the text file
    FILE *file = fopen(text_file, "r");
    if (!file) {
        perror("Error opening text file"); //Showing to the user that the text cannot be opened.
        return 1;
    }
    fread(text, sizeof(char), 1000, file);
    fclose(file);


    // Check if the RS232 port can be opened
    if (CanRS232PortBeOpened() == -1) {
        printf("Unable to open the COM port.\n");
        return 1;
    }

    // Send G-code file to Arduino
    GenerateGCode(text, height, buffer);

    // Close the RS232 port
    CloseRS232Port();
    printf("Communication closed.\n");

    return 0;
}

//The function to load the font data file
void LoadFontData(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening font file");
        exit(1); //Terminating it unsuccessfully as the font file is not found in the file directory
    }
    else {
        printf("Font data loaded successfully.\n");
            char line[100];

        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "999", 3) == 0) {
                int ascii, num_movements;
                sscanf(line, "999 %d %d", &ascii, &num_movements);
                font[ascii].ascii = ascii;
                font[ascii].num_movements = num_movements;

                for (int i = 0; i < num_movements; i++) {
                    fgets(line, sizeof(line), file);
                    sscanf(line, "%d %d %d", &font[ascii].movements[i].x, 
                                            &font[ascii].movements[i].y, 
                                            &font[ascii].movements[i].pen);
                }
            }
        }
    }
    



    fclose(file);
}

//This function adjusts the height, converts the Gcode and ensures that width of the texts being written is within 100mm limit
void GenerateGCode(char *text, float height, char *buffer) {
    float scale = height / 18.0;    // Scale factor for font height
    float x_offset = 0, y_offset = 0; //Initialising the x and y offset variables
    float max_width = 100.0;       // Maximum width of writing area
    int previous_pen_state = -1;   // Track previous pen state (-1 = uninitialized)

    sprintf(buffer, "F1000\nM3\n"); // Initialize G-code
    SendCommands(buffer);

    for (const char *word_start = text; *word_start; ) {
        // This calculates word width
        float word_width = 0;
        const char *p = word_start;
        while (*p && *p != ' ' && *p != '\n') {
            int ascii = (int)*p;
            if (ascii >= 0 && ascii < MAX_FONT_DATA && font[ascii].num_movements > 0) {
                word_width += font[ascii].movements[font[ascii].num_movements - 1].x * scale;
            }
            p++;
        }

        // Check if the word fits in the remaining width
        if (x_offset + word_width > max_width) {
            y_offset -= height; // Move to the next line
            x_offset = 0;      // Reset horizontal position
        }

        // Draw the word
        for (; *word_start && *word_start != ' ' && *word_start != '\n'; word_start++) {
            int ascii = (int)*word_start;

            // Skip undefined characters that is defined or found within the font data file
            if (ascii < 0 || ascii >= MAX_FONT_DATA || font[ascii].num_movements == 0) {
                continue;
            }

            for (int j = 0; j < font[ascii].num_movements; j++) {
                Movement move = font[ascii].movements[j];
                float x = x_offset + move.x * scale;
                float y = y_offset + move.y * scale;
                
                // Only write S0 or S1000 if the pen state changes
                if (move.pen != previous_pen_state) {
                    sprintf(buffer, move.pen == 1 ? "S1000\n" : "S0\n");
                    SendCommands(buffer);
                    previous_pen_state = move.pen; // Update previous pen state
                }

                sprintf(buffer, move.pen == 1 ? "G1 X%.2f Y%.2f\n" : "G0 X%.2f Y%.2f\n", x, y);
                SendCommands(buffer);
            }
            x_offset += font[ascii].movements[font[ascii].num_movements - 1].x * scale;
        }

        // Skip spaces and newlines
        for (; *word_start == ' ' || *word_start == '\n'; word_start++) {
            if (*word_start == '\n') {
                y_offset -= height; // Move to the next line
                x_offset = 0;      // Reset horizontal position
            }
        }

        x_offset += scale*4; // Add spacing between words
    }

    // Ensure the pen is up and return to the origin at the end
    if (previous_pen_state != 0) {
        sprintf(buffer, "S0\n");
        SendCommands(buffer);
    }

    sprintf(buffer, "G0 X0 Y0\n");
    SendCommands(buffer);
}

//This function was already been provided from the original skeleton code.
//This sends the Gcode commands to the robot.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    //WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}