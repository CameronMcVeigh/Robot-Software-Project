#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */

#define Size 1027
void SendCommands (char *buffer );

//Declaring structure to contain one coordiante
struct FontData
{
    float x,y,z;
};

// Structure to hold an array of FontData
struct Multi_FontData {
    struct FontData Font[Size];
};

// Function to populate FontDataArray from the file
void PopulateFontDataArray(struct Multi_FontData *Fonts, const char *filename);

int main()
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];

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

    struct Multi_FontData Fonts;
    int i;
    const char *filename = "SingleStrokeFont.txt"; // Specify the file name

    // Populate the FontData array from the file
    PopulateFontDataArray(&Fonts, filename);

    // Print the data for verification
    for (i = 0; i < Size; i++) {
        printf("FontData %d: %f %f %f\n", i, Fonts.Font[i].x, Fonts.Font[i].y, Fonts.Font[i].z);
    }
    
    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Function to populate FontDataArray
void PopulateFontDataArray(struct Multi_FontData *Fonts, const char *filename) {
    int i;
    FILE *file = fopen(filename, "r"); // Open the file in read mode
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Read the file line by line
    for (i = 0; i < Size ; i++) {
        if (fscanf(file, "%f %f %f", &Fonts->Font[i].x, &Fonts->Font[i].y, &Fonts->Font[i].z) != 3) {
            fprintf(stderr, "Error reading line %d from file.\n", i + 1);
            break;
        }
    }

    fclose(file); // Close the file
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
