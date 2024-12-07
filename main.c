#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */

#define Size 1027
void SendCommands (char *buffer );

//Declaring structure to contain one coordiante with tthe structure of x,y,z
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

// Function to get a  user input between 4 and 10
float GetUserInput();

//Function to apply scaled value
void ScaleCoordinates(struct FontData CharMovementArray[], int NumCharMovements, float scalingFactor);

//Function to retrieve charcter data
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue,struct FontData CharMovementArray[], int *NumCharMovements);

//Function to apply offset
void ApplyOffset(struct FontData charMovementArray[], int NumCharMovements, float PositionX, float PositionY);

//Function to generate G Code
void GenerateGcode(struct FontData charMovementArray[], int NumCharMovements);

//Funcion to calculate word width
float CalculateWordWidth(int WordLength, float CharacterWidth,float scalingFactor);

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
    const float CharacterWidth = 18.0;
    const int LineWidth = 100.0;

    // Populate the FontData array from the font file
    PopulateFontDataArray(&Fonts, filename);

    // Get validated user input
    float userInput = GetUserInput();

    // Calculate the scaling factor using the User input value
    float scalingFactor = userInput / CharacterWidth;  

    char TextFileName[256]; //Creating a buffer to store the name of the text file
    printf("\n Please enter the name of the text file to be drawn by robot: "); // Prompt user to enter name of the text file to be drawn out
    scanf("%s", TextFileName);

    //Opening Textfile and checkign that file can be opened
    FILE *Textfile = fopen(TextFileName, "r");
    if (Textfile == NULL)   //If the file cant be opened
    {
        printf("The text file could not be opened for reading");
        return -1;
    }
    
    float CurrentXPosition = 0.0f; // Track the x position
    float CurrentYPosition = 0.0f; //Track the Y position

    char WordArray[256]; // Creating an array to store each word

    CurrentYPosition -=(CharacterWidth * scalingFactor);   //Move the pen down a line to write out the first line

    while (fscanf(Textfile, "%s", WordArray) != EOF) // Read one word at a time
    {
        int WordLength = (int)strlen(WordArray);    // Find the amount of characters in each word
        float WordWidth = CalculateWordWidth(WordLength, CharacterWidth, scalingFactor);    //Calculate the width of the word

        if (CurrentXPosition +WordWidth > LineWidth) // If the Word doesn't fit on the current line
        {
            CurrentXPosition = 0.0f; // Reset x position abck to the beginning of the line 
            CurrentYPosition -= (CharacterWidth * scalingFactor + 5.0f); // Move the Y cooridnate down onto a new line (wrap word)
        }

        for (int k = 0; k < WordLength; k++)    // For each character in the current word
        {
            int asciiValue = (int)WordArray[k]; // Get the ascii value of the current character
            struct FontData charMovementArray[Size]; //Create an array to hold the current character
            int NumCharMovements = 0;  //Initialise the number of movements of the current character

            NumCharMovements = RetrieveCharacterData(Fonts.Font, asciiValue, charMovementArray, &NumCharMovements); // Retrieve data and number of movements for the current character

            ScaleCoordinates(charMovementArray, NumCharMovements, scalingFactor);   //scale the character by pre-calculated scale factor
            ApplyOffset(charMovementArray, NumCharMovements, CurrentXPosition, CurrentYPosition);   //offset the character from the character before it

            GenerateGcode(charMovementArray, NumCharMovements); // Generate and send Gcode for the current character , line by line

            CurrentXPosition += CharacterWidth * scalingFactor; // Increment X position for the next character
        }

        CurrentXPosition += 18.0f * scalingFactor; // Add space after the word
    }

    sprintf(buffer,"S0\n"); // Move robot arm back to (0,0)
    SendCommands(buffer);

    sprintf(buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    fclose(Textfile);
    CloseRS232Port();
    printf("Com port now closed\n");

    return 0;
}

//function to generate G code
void GenerateGcode(struct FontData charMovementArray[], int NumCharMovements)
{
    char buffer[32]; // Buffer to hold a single line of G-code
    for (int i = 0; i < NumCharMovements; i++) // Loop through each movement
    {
        
        if (charMovementArray[i].z == 1.0) // If pen is down
        {
            sprintf(buffer, "S1000\n"); // Send spindle speed
            SendCommands(buffer); 
            sprintf(buffer, "G1 X%.5f Y%.5f\n", charMovementArray[i].x, charMovementArray[i].y); // G1 command
        }
        else // If pen is up
        {
            sprintf(buffer, "S0\n"); // Send spindle speed
            SendCommands(buffer); 
            sprintf(buffer, "G0 X%.5f Y%.5f\n", charMovementArray[i].x, charMovementArray[i].y); // G0 command
        }

        SendCommands(buffer); // Send the G1 or G0 line
    }
}

// Function to apply offset to the x coordinates of the data
void ApplyOffset(struct FontData charMovementArray[], int NumCharMovements, float CurrentXPosition, float CurrentYPosition)
{
    for (int i = 0; i < NumCharMovements; i++) // Looping through each  
    {
        charMovementArray[i].x +=  CurrentXPosition;
        charMovementArray[i].y +=  CurrentYPosition;
    }
}

// Function to retrieve character data for a given ASCII value
int RetrieveCharacterData(struct FontData FontDataArray[], int asciiValue, struct FontData charMovementArray[], int *NumCharMovements) 
{
    int NumberofLinesToCopy = 0; // Number of movements for the current character

    for (int i = 0; i < Size; i++) 
    {
        // Check for the start of a character's movement data (marked by `999`)
        if (FontDataArray[i].x== 999 && FontDataArray[i].y== asciiValue) 
        {
            // Get the amount of following lines to be read into the outputArray
            NumberofLinesToCopy = (int)FontDataArray[i].z;

            // Copy each line into the outputArray for the specified amount of lines
            for (int k = 0; k <NumberofLinesToCopy; k++) 
            {
                int FontDataline = i + 1 + k; // To prevent the line starting with 999 to be read
            
                charMovementArray[*NumCharMovements] = FontDataArray[FontDataline];  // Copy the line from FontDataArray into OutputMovementArray
                (*NumCharMovements)++; // Increment the number of movements per word
            }
        }
    }

    return NumberofLinesToCopy; // Return the number of movements collected
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
    do 
    {
        printf("Please Enter a font size between 4 and 10 mm: ");   // Prompting user to give an input between 4 and 10 mm
        scanf("%f", &input);                                        

        if (input < 4 || input > 10) 
        {
            printf("Please ensure input value is between 4 and 10mm \n ");
        }
        
    } while (input < 4 || input > 10);      // Only accept a user input in the range of 4 to 10mm
    return input;
}

// Function to scale each coordinate
void ScaleCoordinates(struct FontData charMovementArray[], int NumCharMovements, float scalingFactor) 
{
    for (int i = 0; i < NumCharMovements; i++) 
    {
        charMovementArray[i].x *= scalingFactor; // Scale x coordinate by user input
        charMovementArray[i].y *= scalingFactor; // Scale y coordinate by user input
    }
}

// Function to calculate word width
float CalculateWordWidth(int WordLength, float CharacterWidth,float scalingFactor)
{
    // Calculating the width of the word = number of characters in the word * Size of the character * Scaling factor
    float WordWidth = (float) WordLength * CharacterWidth * scalingFactor;

    return WordWidth;
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