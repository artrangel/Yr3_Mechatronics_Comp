#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rs232.h"
#include "serial.h"

#define MAX_FONT_DATA 256
#define MAX_MOVEMENTS 20
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
int font_loaded = 0;

// Function declarations
void LoadFontData(const char *filename);
void GenerateGCode(char *text, float height, const char *output_file);
void SendCommands(const char *buffer);

int main() {
    char font_file[] = "SingleStrokeFont.txt";
    char text_file[100];
    char output_file[] = "GeneratedGCode.txt";
    char text[1000];
    float height;

    // Load font data
    printf("Loading font data...\n");
    LoadFontData(font_file);
    printf("Font data loaded successfully.\n");

    // Get user input
    printf("Enter text file name: ");
    scanf("%s", text_file);
    printf("Enter height (4-10mm): ");
    scanf("%f", &height);
    if (height < 4 || height > 10) {
        printf("Invalid height. Please use values between 4 and 10mm.\n");
        return 1;
    }

    // Read input text
    FILE *file = fopen(text_file, "r");
    if (!file) {
        perror("Error opening text file");
        return 1;
    }
    fread(text, sizeof(char), 1000, file);
    fclose(file);

    // Generate G-code
    printf("Generating G-code...\n");
    GenerateGCode(text, height, output_file);
    printf("G-code written to %s.\n", output_file);

    return 0;
}

void LoadFontData(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening font file");
        exit(1);
    }

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

    font_loaded = 1;
    fclose(file);
}

void GenerateGCode(char *text, float height, const char *output_file) {
    FILE *file = fopen(output_file, "w");
    if (!file) {
        perror("Error opening output file");
        exit(1);
    }

    fprintf(file, "F1000\nM3\nS0\n"); // Initialize G-code
    float scale = height / 18.0;
    float x_offset = 0, y_offset = 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            y_offset -= 5; // Line spacing
            x_offset = 0;
            continue;
        }

        int ascii = (int)text[i];
        if (font[ascii].num_movements == 0) continue;

        for (int j = 0; j < font[ascii].num_movements; j++) {
            Movement move = font[ascii].movements[j];
            float x = x_offset + move.x * scale;
            float y = y_offset + move.y * scale;
            if (move.pen == 1) {
                fprintf(file, "S1000\n");
                fprintf(file, "G1 X%.2f Y%.2f\n", x, y);
            } else {
                fprintf(file, "S0\n");
                fprintf(file, "G0 X%.2f Y%.2f\n", x, y);
            }
        }

        x_offset += font[ascii].movements[font[ascii].num_movements - 1].x * scale;
    }

    fprintf(file, "S0\nG0 X0 Y0\n"); // Return to origin
    fclose(file);
}

void SendCommands(const char *buffer) {
    PrintBuffer(buffer);
    WaitForReply();
    Sleep(100); // Optional delay
}
