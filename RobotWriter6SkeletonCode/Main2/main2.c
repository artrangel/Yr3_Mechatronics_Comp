#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */

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

// Function declarations
void SendCommands(char* buffer);
FontChar* LoadFontData(const char* filename, int* total_chars);
void GenerateGCode(FontChar* font_data, int total_chars, char character, float scale, int x_offset, int y_offset);
void ProcessText(const char* input_filename, FontChar* font_data, int total_chars, float scale);
float GetScaleFactor(float user_height);

int main() {
    char buffer[100];
    char input_text_file[100];
    float user_height;
    int total_chars;

    // Load font data
    FontChar* font_data = LoadFontData("SingleStrokeFont.txt", &total_chars);
    if (!font_data) {
        printf("Error: Could not load font data.\n");
        return 1;
    }

    // Input text file
    printf("Enter the name of the input text file: ");
    scanf("%s", input_text_file);

    // Input text height
    printf("Enter the desired text height (4-10 mm): ");
    scanf("%f", &user_height);
    if (user_height < 4 || user_height > 10) {
        printf("Error: Text height must be between 4mm and 10mm.\n");
        free(font_data);  // Clean up memory
        return 1;
    }

    float scale = GetScaleFactor(user_height);
    ProcessText(input_text_file, font_data, total_chars, scale);

    if (CanRS232PortBeOpened() == -1) {
        printf("\nUnable to open the COM port (specified in serial.h)\n");
        free(font_data);  // Clean up memory
        return 1;
    }

    // Wake up the robot
    printf("\nAbout to wake up the robot\n");
    sprintf(buffer, "\n");
    PrintBuffer(buffer);
    Sleep(100);
    WaitForDollar();

    printf("\nThe robot is now ready to draw\n");

    // Set robot to drawing mode
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf(buffer, "M3\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);


    // Close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    // Free allocated memory
    for (int i = 0; i < total_chars; i++) {
        free(font_data[i].movements);
    }
    free(font_data);

    return 0;
}

// Load font data
FontChar* LoadFontData(const char* filename, int* total_chars) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file '%s'.\n", filename);
        return NULL;
    }

    FontChar* data = malloc(128 * sizeof(FontChar)); // Allocate for ASCII range
    int index = 0;

    while (!feof(file)) {
        int ascii, num_movements;
        if (fscanf(file, "999 %d %d", &ascii, &num_movements) != 2) {
            break;  // Stop on invalid data
        }

        data[index].ascii = ascii;
        data[index].num_movements = num_movements;
        data[index].movements = malloc(num_movements * sizeof(Movement));

        for (int i = 0; i < num_movements; i++) {
            if (fscanf(file, "%d %d %d", &data[index].movements[i].x, &data[index].movements[i].y, &data[index].movements[i].pen) != 3) {
                printf("Error reading font data.\n");
                fclose(file);
                free(data);
                return NULL;
            }
        }
        index++;
    }

    fclose(file);
    *total_chars = index;
    return data;
}

// Generate G-code
void GenerateGCode(FontChar* font_data, int total_chars, char character, float scale, int x_offset, int y_offset) {
    char buffer[100];
    for (int i = 0; i < total_chars; i++) {
        if (font_data[i].ascii == character) {
            for (int j = 0; j < font_data[i].num_movements; j++) {
                int x = (int)(font_data[i].movements[j].x * scale) + x_offset;
                int y = (int)(font_data[i].movements[j].y * scale) + y_offset;

                if (font_data[i].movements[j].pen == 1) {
                    sprintf(buffer, "S1000\nG1 X%d Y%d\n", x, y);
                } else {
                    sprintf(buffer, "S0\nG0 X%d Y%d\n", x, y);
                }

                // Print each G-code command to the terminal
                printf("Generated G-code: %s", buffer);

                SendCommands(buffer);
            }
            break;
        }
    }
}

// Process text
void ProcessText(const char* input_filename, FontChar* font_data, int total_chars, float scale) {
    FILE* file = fopen(input_filename, "r");
    if (!file) {
        printf("Error: Could not open input text file '%s'.\n", input_filename);
        return;
    }

    char word[100];
    int x_offset = 0, y_offset = 0;

    while (fscanf(file, "%s", word) != EOF) {
        for (int i = 0; word[i] != '\0'; i++) {
            GenerateGCode(font_data, total_chars, word[i], scale, x_offset, y_offset);
            x_offset += (int)(18 * scale); // Adjust for next character
        }
        y_offset -= 5; // Line spacing
        x_offset = 0;  // Reset x_offset for new line
    }

    fclose(file);
}

// Send commands
void SendCommands(char* buffer) {
    // Display the G-code in the terminal
    printf("Generated G-code: %s", buffer);
    
    #if !SIMULATE_OUTPUT
        // Only send to the robot if simulation is disabled
        PrintBuffer(buffer);
        WaitForReply();
        Sleep(100);
    #endif
}

// Get scale factor
float GetScaleFactor(float user_height) {
    return user_height / 18.0; // Assuming base height in font data is 18 units
}
