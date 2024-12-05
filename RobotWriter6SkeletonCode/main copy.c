#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200 /* 115200 baud */

// Font data structures
typedef struct {
    int x;
    int y;
    int pen;
} Movement;

typedef struct {
    int ascii;
    int num_movements;
    Movement* movements;
} FontChar;

// Global font data
FontChar* font_data;
int total_chars = 0;

// Function declarations
void SendCommands(char* buffer);
FontChar* loadFontData(const char* filename, int* total_chars);
void generateGCodeForChar(FontChar character, float scale, int x_offset, int y_offset);

int main() {
    char buffer[100];

    // Load font data
    font_data = loadFontData("SingleStrokeFont.txt", &total_chars);
    if (!font_data) {
        printf("Error: Could not load font data.\n");
        exit(1);
    }

    // Initialize RS232 communication
    if (CanRS232PortBeOpened() == -1) {
        printf("Unable to open the COM port (specified in serial.h).\n");
        exit(1);
    }

    printf("Robot initialized.\n");

    // Prepare robot for drawing
    SendCommands("G1 X0 Y0 F1000\n");
    SendCommands("M3\n");
    SendCommands("S0\n");

    // Read input text file
    FILE* input_file = fopen("test.txt", "r");
    if (!input_file) {
        printf("Error: Could not open input file.\n");
        exit(1);
    }

    char word[100];
    int x_offset = 0, y_offset = 0;
    float scale = 8.0 / 18.0; // Example scale for 8mm height

    while (fscanf(input_file, "%s", word) != EOF) {
        for (int i = 0; word[i] != '\0'; i++) {
            for (int j = 0; j < total_chars; j++) {
                if (font_data[j].ascii == word[i]) {
                    generateGCodeForChar(font_data[j], scale, x_offset, y_offset);
                    x_offset += (int)(18 * scale); // Adjust offset for next character
                    break;
                }
            }
        }
        y_offset -= 5; // New line offset
        x_offset = 0;
    }

    fclose(input_file);

    // Close the communication port
    CloseRS232Port();
    printf("Com port now closed.\n");

    // Free allocated memory
    for (int i = 0; i < total_chars; i++) {
        free(font_data[i].movements);
    }
    free(font_data);

    return 0;
}

// Load font data from a file
FontChar* loadFontData(const char* filename, int* total_chars) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file.\n");
        return NULL;
    }

    FontChar* data = malloc(128 * sizeof(FontChar)); // Assuming ASCII range
    int index = 0;

    while (!feof(file)) {
        int ascii, num_movements;
        fscanf(file, "999 %d %d", &ascii, &num_movements);
        data[index].ascii = ascii;
        data[index].num_movements = num_movements;
        data[index].movements = malloc(num_movements * sizeof(Movement));

        for (int i = 0; i < num_movements; i++) {
            fscanf(file, "%d %d %d", &data[index].movements[i].x, &data[index].movements[i].y, &data[index].movements[i].pen);
        }
        index++;
    }

    fclose(file);
    *total_chars = index;
    return data;
}

// Generate G-code for a single character
void generateGCodeForChar(FontChar character, float scale, int x_offset, int y_offset) {
    char buffer[100];

    for (int i = 0; i < character.num_movements; i++) {
        int x = (int)(character.movements[i].x * scale) + x_offset;
        int y = (int)(character.movements[i].y * scale) + y_offset;

        if (character.movements[i].pen == 1) {
            sprintf(buffer, "S1000\nG1 X%d Y%d\n", x, y);
        } else {
            sprintf(buffer, "S0\nG0 X%d Y%d\n", x, y);
        }

        SendCommands(buffer);
    }
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}