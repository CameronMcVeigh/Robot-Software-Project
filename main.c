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
float GetUserInput();

//Function to apply scaled value
void ScaleCoordinates(struct FontData outputMovementArray[], int count, float scalingFactor);

//Function to check if file is open
void CheckFileIsOpen(const char *filename);

//Function to count number of characters in the file
int CountCharactersInFile(const char *filename);

//Function to store text from file into array of characters
char* ReadTextFileIntoArray(const char *filename, int characterCount);

//Function to retrieve character data
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue,struct FontData outputMovementArray[], int *Numberofmovements);

//Function to apply offset
void ApplyOffset(struct FontData outputMovementArray[], int count, float PositionX, float PositionY);

//Function to generate G Code
void GenerateGcode(struct FontData outputMovementArray[], char GcodeArray[], int Numberofmovements);


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
    const int CharacterSize = 18.0;
    const int linespace = 100.0;

    // Populate the FontData array from the font file
    PopulateFontDataArray(&Fonts, filename);

    // Get validated user input
    float userInput = GetUserInput();

    // Calculate the scaling factor
    float scalingFactor = userInput / CharacterSize; // Calculating a scaling factor from user input 

    // Ask the user for the name of the second text file
    char TextFileName[256]; //Creating a buffer to store the name of the text file
    printf("\nPlease enter the name of the text file to be drawn: "); // Prompt user to enter name of the text file to be read
    scanf("%s", TextFileName);

    // Check if the second file can be opened
    CheckFileIsOpen(TextFileName);  

    // Counting the number of characters in the text file so I can loop through each character
    int characterCount = CountCharactersInFile(TextFileName);

    // Create an array and read the file content into it
    char *charArray = ReadTextFileIntoArray(TextFileName, characterCount);

    // Buffer to store retrieved data
    struct FontData outputMovementArray[Size];  //Creating Array to store the robot movements for each word
    int Numberofmovements = 0;                  //integer used to store the number of robot movements for each word
 
    float CurrentXPosition = 0.0f; // Track the x position
    float CurrentYPosition = 0.0f; // track the Y position

    for (int i=0; i< characterCount;) // Looping through the amount of characters in the text file
    {

        char wordBuffer[256]; // Creating a Buffer to store one word
        int wordLength = 0;   // Integer to store the length of the word

        while (i < characterCount && charArray[i] != 32)    //For each character until we reach a space
        { // Not a space
            wordBuffer[wordLength] = charArray[i];      //Adding each character to a word 
            i++;
            wordLength++;
        }

        // Skip the space
        if (i < characterCount && charArray[i] == 32) 
        {
            i++;
        }

        // Calculate the word's width
        float WordWidth = 0.0f;

        WordWidth = wordLength* CharacterSize * scalingFactor;

        // Check if the word fits on the current line
        if (CurrentXPosition + WordWidth > linespace) //if the Word size cannot fit on the current line
        {
            CurrentXPosition = 0.0f;                            //Reset the xPosition back to the beginning of the line
            CurrentYPosition -= (CharacterSize * scalingFactor + 5.0f); // Move the Y position down by the size of the letter with a 5mm gap (new line) 
        }


        struct FontData outputMovementArray[Size]; // Creating a Array for the output Movements for each character
        int Numberofmovements = 0;

        for (int j = 0; j < wordLength; j++) //Looping through each character in the word
        {
            int asciiValue = (int)wordBuffer[j]; // Geting asciiValue for each character
            int charMovements = RetrieveCharacterData(Fonts.Font, asciiValue, outputMovementArray, &Numberofmovements); // retrieve character data for each character

            ScaleCoordinates(&outputMovementArray[Numberofmovements - charMovements], charMovements, scalingFactor);    //apply a scale factor to character data to the current character only
            ApplyOffset(&outputMovementArray[Numberofmovements - charMovements], charMovements, CurrentXPosition, CurrentYPosition);//Offset each character within the outputmovement array

            // Increment X position for the next character
            CurrentXPosition += CharacterSize * scalingFactor;
        }

        char *GcodeArray;
        GcodeArray = (char *) calloc (Size*64, sizeof(char));   //dynamically allocating size of Gcode
        //check that memory could be allocated
        if ( GcodeArray == NULL)
        {
            printf("\nMemory could not be allocated - terminatin");
            return -1;
        }
        GenerateGcode(outputMovementArray, GcodeArray, Numberofmovements);
        //Send the G codees to the arduino
        SendCommands(GcodeArray);
        
        free(GcodeArray); // free memory to allow space for new word

        // Add a space after the word
        CurrentXPosition += 5.0f * scalingFactor;
    }

    //freeing memory
    free(charArray);

    CloseRS232Port();
    printf("Com port now closed\n");

    return 0;
}


//Function to generate G code
void GenerateGcode(struct FontData outputMovementArray[], char GcodeArray[], int Numberofmovements)
{
    int GcodePosition = 0; //Tracking the GcodePosition so i can move through array

    for (int i=0; i < Numberofmovements; i++) // loop through each struct in outputMovementArray
    {
        if(outputMovementArray[i].z == 1.0)      // If pen is down
        {
            GcodePosition += sprintf (&GcodeArray[GcodePosition], "S1000\n");//Print with S1000 and G1
            GcodePosition += sprintf (&GcodeArray[GcodePosition], "G1 X%.2f Y%.2f \n ", outputMovementArray[i].x, outputMovementArray[i].y);
        }
        else //if pen is up
        {
            GcodePosition += sprintf (&GcodeArray[GcodePosition], "S0\n"); // print with S0 and G0
            GcodePosition += sprintf (&GcodeArray[GcodePosition], "G0 X%.2f Y%.2f\n", outputMovementArray[i].x, outputMovementArray[i].y);
        }
    }
    GcodeArray[GcodePosition] = '\0'; 
}

// Function to apply offset to the x coordinates of the data
void ApplyOffset(struct FontData outputMovementArray[], int count, float CurrentXPosition, float CurrentYPosition)
{
    for (int i = 0; i < count; i++) // looping through each  
    {
        outputMovementArray[i].x +=  CurrentXPosition;
        outputMovementArray[i].y +=  CurrentYPosition;
    }
}

// Function to retrieve character data for a given ASCII value
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue, struct FontData outputMovementArray[], int *Numberofmovements) 
{
    int count = 0; // Number of movements for the current character

    for (int i = 0; i < Size; i++) 
    {
        // Check for the start of a character's movement data (marked by `999`)
        if (FontDataArray[i].x== 999 && FontDataArray[i].y== asciiValue) 
        {
            // Get the amount of following lines to be read into the outputArray
            count = (int)FontDataArray[i].z;

            // Copy each line into the outputArray for the specified amount of line
            for (int k = 0; k < count; k++) 
            {
                int index = i + 1 + k; // Offset from the current `999` line
            
                outputMovementArray[*Numberofmovements] = FontDataArray[index];
                (*Numberofmovements)++;
            }
           // break; // Exit after processing the current character
        }
    }

    return count; // Return the number of movements collected
}


// Function to check if a file can be opened
void CheckFileIsOpen(const char *filename) 
{
    FILE *file = fopen(filename, "r"); // Attempt to open the file
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }
    fclose(file); // Close the file after the check
}

// Function to populate FontDataArray
void PopulateFontDataArray(struct Multi_FontData *Fonts, const char *filename) {
    int i;
    FILE *file = fopen(filename, "r"); // Open the file in read mode
    if (file == NULL) 
    {
        printf("Error opening StrokeFontData file");
    }

    // Read the file line by line
    for (i = 0; i < Size ; i++) 
    {
        fscanf(file, "%f %f %f", &Fonts->Font[i].x, &Fonts->Font[i].y, &Fonts->Font[i].z);
    }

    fclose(file); // Close the file
}

// Function to get and validate user input
float GetUserInput() 
{
    float input;
    do {
        printf("Enter a font size between 4 and 10mm: ");
        if (scanf("%f", &input) != 1) 
        {  
            while (getchar() != '\n '); // Clear invalid input from buffer
            printf("The input Must be a number! \n ");
            continue;
        }
        if (input < 4 || input > 10) 
        {
            printf("Please ensure input is in the correct range \n ");
        }
    } while (input < 4 || input > 10);      // only accept a user input in the range of 4 to 10mm
    return input;
}


//Function to scale each coordiante
void ScaleCoordinates(struct FontData outputMovementArray[], int count, float scalingFactor) 
{
    for (int i = 0; i < count; i++) 
    {
        outputMovementArray[i].x *= scalingFactor; //scale x cooridante by user input
        outputMovementArray[i].y *= scalingFactor; // scale y coordinate by user input
    }
}

int CountCharactersInFile(const char *filename) 
{
    FILE *file = fopen(filename, "r");  //open file
    if (file == NULL)                   //Check that file can be opened
    {                 
        printf("Error opening file");
        return -1;
    }

    int count = 0;
    while (fgetc(file) != EOF)  //until we have reached the end of the file
    { 
        count++;        //count number of characters in the file
    }

    fclose(file); // Close the file
    return count; // return the number of characters in the file
}

// Function to read file content into an array
char* ReadTextFileIntoArray(const char *filename, int characterCount) 
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) 
    {
        printf("Error opening text file");
    }

    // Allocate memory for the character array
    char *charArray = (char *)malloc(characterCount * sizeof(char));

    // Read characters from the file into the array
    for (int i = 0; i < characterCount; i++) 
    {
        int character = fgetc(file);
        charArray[i] = (char)character;
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
