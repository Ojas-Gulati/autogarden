// Example program
#include <iostream>
#include <string>

using namespace std;

string numtostring(long input, int bytes)
{
    string output = "";
    for (int i = 0; i < (bytes * 2); i++)
    {
        int nextone = (input >> ((bytes * 2) - i - 1) * 4) % 16;
        if (nextone <= 9)
        {
            output.push_back((char)(nextone + (int)('0')));
        }
        else
        {
            output.push_back((char)(nextone - 10 + (int)('A')));
        }
    }
    return output;
}

// ST{s/m}
// WF(wifissid|64)(wifipassword|128)
// AD<macaddress>[112233|3](slaveboard1|23)
// SL[255|1][0|1][180|1][30|1]
// AD

int main()
{
    string input = "";
    cin >> input;
    string output = "";
    bool raw = false;    // <>
    bool number = false; // [num|bytes]
    bool padded = false; // (|num)
    bool readingnumber = false;
    bool readingoptionnumber = false;

    int charsinpad = 0;
    int numbersofar = 0;
    int optionnumber = 0;
    for (int i = 0; i < input.size(); i++)
    {
        bool noaction = false;
        if (input[i] == '\\')
        {
            i = i + 1;
            noaction = true;
        }
        else
        {
            if (input[i] == '<' && !raw)
            {
                raw = true;
            }
            else if (input[i] == '>' && raw)
            {
                raw = false;
            }
            else if (input[i] == '[' && !number)
            {
                number = true;
                numbersofar = 0;
                readingnumber = true;
            }
            else if (input[i] == ']' && number)
            {
                number = false;
                readingnumber = false;
                readingoptionnumber = false;
                // add the number
                output = output + numtostring(numbersofar, optionnumber);
            }
            else if (input[i] == '(' && !padded)
            {
                padded = true;
                charsinpad = 0;
            }
            else if (input[i] == ')')
            {
                padded = false;
                // cout << optionnumber << " " << charsinpad << endl;
                // add the padding
                for (int j = 0; j < (optionnumber - charsinpad); j++) {
                    output = output + "00";
                }
            }
            else if (input[i] == '|')
            {
                readingnumber = false;
                readingoptionnumber = true;
                padded = false;
                optionnumber = 0;
            }
            else
            {
                noaction = true;
            }
        }
        if (noaction)
        {
            if (raw)
            {
                output.push_back(input[i]);
            }
            else if (readingnumber)
            {
                numbersofar *= 10;
                numbersofar += (int)(input[i]) - (int)('0');
            }
            else if (readingoptionnumber)
            {
                optionnumber *= 10;
                optionnumber += (int)(input[i]) - (int)('0');
            }
            else if (padded)
            {
                charsinpad += 1;
                output = output + numtostring((int)(input[i]), 1);
            }
            else
            {
                output = output + numtostring((int)(input[i]), 1);
            }
        }
        // cout << output << endl;
    }
    cout << output;
}