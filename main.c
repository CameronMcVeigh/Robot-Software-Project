#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */

#define Size 1027
void SendCommands (char *buffer );

//Declaring structure to contain one coordiante with specifications for x,y,z
struct FontData
{
    float x,y,z;
};

// Structure to hold an array of FontData
struct Multi_FontData
{
    struct FontData Font[Size];
};

// Function to populate FontDataArray from the file
void PopulateFontDataArray(struct Multi_FontData *Fonts, const char *filename);

// Function to get user input between 4 and 10
int GetValidatedInput();

//Function to apply scaled value
void ScaleCoordinates(struct FontData outputMovementArray[], int count, float scalingFactor);

//Function to check if file is open
void CheckFileIsOpen(const char *filename);

//Function to count number of characters in the file
int CountCharactersInFile(const char *filename);

//Function to store text from file into array of characters
char* ReadFileIntoArray(const char *filename, int characterCount);

//Function to retrieve character data
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue, struct FontData outputMovementArray[]);

//Function to apply offset
void ApplyOffset(struct FontData outputMovementArray[], int count, float PositionX, float PositionY);

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
    const char *filename = "SingleStrokeFont.txt"; // Specify the file name

    // Populate the FontData array from the font file
    PopulateFontDataArray(&Fonts, filename);

    // Get validated user input
    int userInput = GetValidatedInput();
    printf("You entered: %d\n", userInput); // print the user input for verification

    // Calculate the scaling factor
    float scalingFactor = userInput / 18.0; // Ensure floating-point division
    printf("Scaling Factor: %.2f\n", scalingFactor); // prrint scaling factor for verification

    // Ask the user for the name of the second text file
    char TextFileName[256]; //Creating a buffer to store the second file name
    printf("\nPlease enter the name of the text file to be drawn: "); // Prompt user to enter name of the text file to be read
    scanf("%s", TextFileName);

    // Check if the second file can be opened
    CheckFileIsOpen(TextFileName);

    // After checking the second file can be opened count the number of character in the text file, for the next loop
    int characterCount = CountCharactersInFile(TextFileName);

    // Create an array and read the file content into it
    char *charArray = ReadFileIntoArray(TextFileName, characterCount);

    // Buffer to store retrieved data
    struct FontData outputMovementArray[Size];
    int Numberofmovements = 0;

    float CurrentXPosition = 0.0f; // Track the x position
    float CurrentYPosition = 0.0f; // track the Y position

    // Process each character in the file
    for (int i = 0; i < characterCount; i++) 
    {
        int asciiValue = (int)charArray[i]; // Get ASCII value of the current character

        if (asciiValue == 32) 
        { // If a space or newline is encountered
            printf("Retrieved word data. Moving to next word... \n"); // replace this with processing of word (ie convert to G code then send to Arduino)
            CurrentXPosition += 5.0f * scalingFactor; // Increment the offset for the next word
        }

        // Retrieve the character data for the current ASCII value
        int Numberofmovements = RetrieveCharacterData(Fonts.Font, asciiValue, outputMovementArray);

        // Scale the coordinates 
        ScaleCoordinates(outputMovementArray, Numberofmovements, scalingFactor);

        // Apply the offset to the x coordinates
        ApplyOffset(outputMovementArray, Numberofmovements, CurrentXPosition, CurrentYPosition);

        // Print the retrieved data to verify its contents
        for (int j = 0; j < Numberofmovements; j++) {
            printf("%.2f %.2f %.2f\n", outputMovementArray[j].x, outputMovementArray[j].y, outputMovementArray[j].z);
        }

        // Increment the offset for the next character
        CurrentXPosition += 18.0f * scalingFactor;

        // Clear the outputMovementArray for the next word
        for (int j = 0; j < Numberofmovements; j++) 
        {
            outputMovementArray[j] = (struct FontData){0.0f, 0.0f, 0.0f};
        }
    }

    // Free allocated memory from the text file
    free(charArray);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Function to apply offset to the x coordinates of the data
void ApplyOffset(struct FontData outputMovementArray[], int count, float PositionX, float PositionY)
{
    for (int i = 0; i < count; i++) {
        outputMovementArray[i].x += PositionX;
        outputMovementArray[i].y += PositionY;
    }
}

// Function to retrieve character data for a given ASCII value
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue, struct FontData outputMovementArray[]) 
{
    int outputindex;
    int count;
    for (int i = 0; i < Size; i++) 
    {
        if (FontDataArray[i].x == 999 && FontDataArray[i].y == asciiValue) 
        {
            count = FontDataArray[i].z;
            for (int k=1; k < count; k++)
            {
                outputMovementArray[outputindex++] = FontDataArray[i + 1 + k];
            }
        }
    }
    return count;
}

// Function to check if a file can be opened
void CheckFileIsOpen(const char *filename) {
    FILE *file = fopen(filename, "r"); // Attempt to open the file
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    printf("The file '%s' was opened successfully.\n", filename);
    fclose(file); // Close the file after the check
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

// Function to get and validate user input
int GetValidatedInput() {
    int input;
    do {
        printf("Enter a font size between 4 and 10: ");
        if (scanf("%d", &input) != 1) {
            while (getchar() != '\n'); // Clear invalid input from buffer
            printf("Invalid input. Please enter an integer.\n");
            continue;
        }
        if (input < 4 || input > 10) {
            printf("Invalid range. ");
        }
    } while (input < 4 || input > 10);
    return input;
}

//Function to scale each coordiante
void ScaleCoordinates(struct FontData outputMovementArray[], int count, float scalingFactor) 
{
    for (int i = 0; i < count; i++) {
        outputMovementArray[i].x *= scalingFactor;
        outputMovementArray[i].y *= scalingFactor;
        // Z coordinate remains unchanged
    }
}

int CountCharactersInFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file");
        return -1;
    }

    int count = 0;
    while (fgetc(file) != EOF) { // Read character by character
        count++;
    }

    fclose(file); // Close the file
    return count;
}

// Function to read file content into an array
char* ReadFileIntoArray(const char *filename, int characterCount) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Allocate memory for the character array
    char *charArray = (char *)malloc(characterCount * sizeof(char));

    // Read characters from the file into the array
    for (int i = 0; i < characterCount; i++) 
    {
        int ch = fgetc(file);
        charArray[i] = (char)ch;
    }

    fclose(file);
    return charArray;
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
