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

//Created a structure just to store the data of when the robot should lift the pen and where to move next
typedef struct {
    int x;
    int y;
    int pen;
} Movement;

//Created a structure to store the font data details and this also includes the Movement structure 
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

    // Get the user input for desired height
    printf("Enter height (4-10mm): ");
    scanf("%f", &height);

    //A while loop to repeatedly ask the user to input a height within the range of 4mm-10mm
    while (height < 4 || height > 10) {
        printf("Invalid height. Please use values between 4 and 10mm.\n Enter height again (4-10mm):");
        scanf("%f", &height);
    }

    // Get the user input for text file name
    printf("Enter text file name: ");
    scanf("%s", text_file);
    
    // Reading the text file
    FILE *file = fopen(text_file, "r");
    if (!file) {
        perror("Error opening text file"); //Showing to the user that the text cannot be opened.
        return 1;
    }

    // Read the text from the file into the buffer
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
    // Open the font data file for reading
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening font file"); // Display an error if the file cannot be opened
        exit(1); // Exit the program if the file cannot be found
    }
    else {
        printf("Font data loaded successfully.\n");  // Notify the user that the font data is being loaded
            char line[100]; // This is to store lines read from the file

        // Using a while loop to read each line of the font file
        while (fgets(line, sizeof(line), file)) {

            // Check for the marker indicating the start of a character's font data which is '999' for the first interger
            if (strncmp(line, "999", 3) == 0) {
                int ascii, num_movements; // Variables to store ASCII value and movement count
                sscanf(line, "999 %d %d", &ascii, &num_movements);
                font[ascii].ascii = ascii; // Store the ASCII value
                font[ascii].num_movements = num_movements; // Store the number of movements

                // Using a for loop to read each movement for the character
                for (int i = 0; i < num_movements; i++) {
                    fgets(line, sizeof(line), file);
                    sscanf(line, "%d %d %d", &font[ascii].movements[i].x, // Parse x-coordinate
                                            &font[ascii].movements[i].y, // Parse y-coordinate
                                            &font[ascii].movements[i].pen); // Parse pen state
                }
            }
        }
    }
    



    fclose(file);
}

//This function adjusts the height and converts the Gcode
//This function also ensures that the width of the texts being written is within 100mm limit
void GenerateGCode(char *text, float height, char *buffer) {

    float x_offset = 0, y_offset = 0; //Initialising the x and y offset variables
    float max_width = 100.0;       // Maximum width of writing area
    int previous_pen_state = -1;   // Track previous pen state (-1 = uninitialized)

    float scale = height / 18.0;    // Scale factor for font height
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
            else {
                // Error handling for invalid or undefined character
                fprintf(stderr, "Error: Invalid or undefined character '%c' (ASCII: %d) encountered.\n", *p, ascii);
                return 0;
            }
            p++;
        }

        // Checks if the word fits in the remaining width
        if (x_offset + word_width > max_width) {
            y_offset -= height; // Move to the next line
            x_offset = 0;      // Reset horizontal position
        }

        // A for loop to drawing the word
        for (; *word_start && *word_start != ' ' && *word_start != '\n'; word_start++) {
            int ascii = (int)*word_start;

            // Skip undefined characters that is not found within the font data file
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
            // Update the x-offset for the next character
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
// Function to send G-code commands to the robot or emulator
void SendCommands(char *buffer) {
    PrintBuffer(&buffer[0]); // Send the buffer contents via RS232
    Sleep(100); // Optional delay to stabilise communication
}
