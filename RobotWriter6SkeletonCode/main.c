#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
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
void GenerateGCode(FontChar character, float scale, int x_offset, int y_offset);
void ProcessText(const char* input_filename, float scale);
float GetScaleFactor(float user_height);

int main()
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];
    
    char input_text_file[100];
    float user_height; 
    int total_chars;

    FontChar* font_data = LoadFontData("SingleStrokeFont.txt", &total_chars);
    if (!font_data) {
        printf("Error: Could not load font data.\n");
        return 1;
    }

    printf("Enter the name of the input text file: ");
    scanf("%s", input_text_file);


    printf("Enter the desired text height (4-10 mm): ");
    scanf("%f", &user_height);
    if (user_height < 4 || user_height > 10) {
        printf("Error: Text height must be between 4mm and 10mm.\n");
        return 1;
    }
    float scale = GetScaleFactor(user_height);

    ProcessText(input_text_file, scale);


    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);


    // These are sample commands to draw out some information - these are the ones you will be generating.
    //sprintf (buffer, "G0 X-13.41849 Y0.000\n");
    //SendCommands(buffer);
    //sprintf (buffer, "S1000\n");
    //SendCommands(buffer);
    //sprintf (buffer, "G1 X-13.41849 Y-4.28041\n");
    //SendCommands(buffer);
    //sprintf (buffer, "G1 X-13.41849 Y0.0000\n");
    //SendCommands(buffer);
    //sprintf (buffer, "G1 X-13.41089 Y4.28041\n");
    //SendCommands(buffer);
    //sprintf (buffer, "S0\n");
    //SendCommands(buffer);
    //sprintf (buffer, "G0 X-7.17524 Y0\n");
    //SendCommands(buffer);
    //sprintf (buffer, "S1000\n");
    //SendCommands(buffer);
    //sprintf (buffer, "G0 X0 Y0\n");
    //SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    // Free allocated memory
    for (int i = 0; i < total_chars; i++) {
        free(font_data[i].movements);
    }
    free(font_data);

    return (0);
}


// Load font data from the file
FontChar* LoadFontData(const char* filename, int* total_chars) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file '%s'.\n", filename);
        return NULL;
    }

    FontChar* data = malloc(128 * sizeof(FontChar)); // Assume ASCII range
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


// Generate G-code for a character
void GenerateGCode(FontChar character, float scale, int x_offset, int y_offset) {
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


// Process text file and generate G-code for each character
void ProcessText(const char* input_filename, float scale) {
    FILE* file = fopen(input_filename, "r");
    if (!file) {
        printf("Error: Could not open input text file '%s'.\n", input_filename);
        return;
    }

    char word[100];
    int x_offset = 0, y_offset = 0;

    while (fscanf(file, "%s", word) != EOF) {
        for (int i = 0; word[i] != '\0'; i++) {
            for (int j = 0; j < total_chars; j++) {
                if (font_data[j].ascii == word[i]) {
                    GenerateGCode(font_data[j], scale, x_offset, y_offset);
                    x_offset += (int)(18 * scale); // Adjust for next character
                    break;
                }
            }
        }
        y_offset -= 5; // Line spacing
        x_offset = 0;  // Reset x_offset for new line
    }

    fclose(file);
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

float GetScaleFactor(float user_height) {
    return user_height / 18.0; // Assuming base height in font data is 18 units
}