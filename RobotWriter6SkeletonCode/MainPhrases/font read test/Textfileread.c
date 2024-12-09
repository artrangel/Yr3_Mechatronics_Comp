#include <stdio.h>
#include <stdlib.h>

// Define a structure to hold three integers
struct FontData
 {
    int first;
    int second;
    int third;
};

int main(void) 
{
    // Declare an array of FontData with size 1027
    struct FontData fontDataArray[1027];

    // Open the font file
    FILE *file = fopen("SingleStrokeFont.txt","r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Use a for loop to read each line into the array
    for (int i = 0; i < 1027; i++) {
        // Read three integers from each line into the array
        if (fscanf(file, "%d %d %d", &fontDataArray[i].first, &fontDataArray[i].second, &fontDataArray[i].third) != 3) {
            printf("Error reading data at line %d\n", i + 1);
            break; // Exit if there's an error reading a line
        }
    }

    // Close the file
    fclose(file);

    // Display the first 10 entries in the structure array
    printf("First 10 entries in the structure array:\n");
    for (int i = 0; i < 10; i++) {
        printf("%d %d %d\n", fontDataArray[i].first, fontDataArray[i].second, fontDataArray[i].third);
    }

    return 0;
}
