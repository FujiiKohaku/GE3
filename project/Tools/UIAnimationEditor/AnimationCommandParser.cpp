#include "AnimationCommandParser.h"
#include <cctype>
#include <cstdlib>
#include <sstream>

bool AnimationCommandParser::Parse(const std::string& commandText, std::vector<AnimationCommand>* commands, std::string* errorMessage) const
{
    if (commands == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "AnimationCommandParser output is null.";
        }
        return false;
    }

    commands->clear();

    std::istringstream stream(commandText);
    std::string line;
    AnimationCommand currentCommand;
    bool hasCurrentCommand = false;
    int lineNumber = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;
        std::string cleanedLine = RemoveCommandNoise(line);

        if (cleanedLine.empty()) {
            continue;
        }

        AnimationCommandType commandType = AnimationCommandType::Move;

        if (IsCommandName(cleanedLine, &commandType)) {
            if (hasCurrentCommand) {
                commands->push_back(currentCommand);
            }

            currentCommand = AnimationCommand();
            currentCommand.type = commandType;
            hasCurrentCommand = true;
            continue;
        }

        if (!hasCurrentCommand) {
            if (errorMessage != nullptr) {
                std::ostringstream message;
                message << "Command parse failed at line " << lineNumber << ": expected command name before property.";
                *errorMessage = message.str();
            }
            return false;
        }

        if (!ParsePropertyLine(cleanedLine, &currentCommand, errorMessage)) {
            if (errorMessage != nullptr) {
                std::ostringstream message;
                message << "Command parse failed at line " << lineNumber << ": " << *errorMessage;
                *errorMessage = message.str();
            }
            return false;
        }
    }

    if (hasCurrentCommand) {
        commands->push_back(currentCommand);
    }

    if (commands->empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "No animation commands were found.";
        }
        return false;
    }

    return true;
}

bool AnimationCommandParser::IsCommandName(const std::string& line, AnimationCommandType* type) const
{
    std::string lowerLine = ToLower(line);

    if (lowerLine == "move") {
        if (type != nullptr) {
            *type = AnimationCommandType::Move;
        }
        return true;
    }
    if (lowerLine == "fade") {
        if (type != nullptr) {
            *type = AnimationCommandType::Fade;
        }
        return true;
    }
    if (lowerLine == "alpha") {
        if (type != nullptr) {
            *type = AnimationCommandType::Fade;
        }
        return true;
    }
    if (lowerLine == "scale") {
        if (type != nullptr) {
            *type = AnimationCommandType::Scale;
        }
        return true;
    }
    if (lowerLine == "rotate") {
        if (type != nullptr) {
            *type = AnimationCommandType::Rotate;
        }
        return true;
    }
    if (lowerLine == "rotation") {
        if (type != nullptr) {
            *type = AnimationCommandType::Rotate;
        }
        return true;
    }
    if (lowerLine == "color") {
        if (type != nullptr) {
            *type = AnimationCommandType::Color;
        }
        return true;
    }

    return false;
}

bool AnimationCommandParser::ParsePropertyLine(const std::string& line, AnimationCommand* command, std::string* errorMessage) const
{
    if (command == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "command is null.";
        }
        return false;
    }

    std::size_t openIndex = line.find('(');
    std::size_t closeIndex = line.rfind(')');

    if (openIndex == std::string::npos || closeIndex == std::string::npos || closeIndex <= openIndex) {
        if (errorMessage != nullptr) {
            *errorMessage = "property must use Name(value) format.";
        }
        return false;
    }

    std::string propertyName = ToLower(Trim(line.substr(0, openIndex)));
    std::string valueText = Trim(line.substr(openIndex + 1, closeIndex - openIndex - 1));

    if (propertyName == "start") {
        if (!ParseFloatList(valueText, &command->startValues)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Start values are invalid.";
            }
            return false;
        }
        command->hasStart = true;
        return true;
    }

    if (propertyName == "end") {
        if (!ParseFloatList(valueText, &command->endValues)) {
            if (errorMessage != nullptr) {
                *errorMessage = "End values are invalid.";
            }
            return false;
        }
        command->hasEnd = true;
        return true;
    }

    if (propertyName == "duration") {
        std::vector<float> values;

        if (!ParseFloatList(valueText, &values) || values.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Duration value is invalid.";
            }
            return false;
        }

        command->durationSeconds = values[0];

        if (command->durationSeconds <= 0.0f) {
            command->durationSeconds = 0.1f;
        }

        return true;
    }

    if (propertyName == "at" || propertyName == "time" || propertyName == "delay") {
        std::vector<float> values;

        if (!ParseFloatList(valueText, &values) || values.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "At value is invalid.";
            }
            return false;
        }

        command->atSeconds = values[0];

        if (command->atSeconds < 0.0f) {
            command->atSeconds = 0.0f;
        }

        return true;
    }

    if (propertyName == "ease") {
        UIEditorInterpolation interpolation = UIEditorInterpolation::Linear;

        if (!ParseInterpolation(valueText, &interpolation)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Unknown easing: " + valueText;
            }
            return false;
        }

        command->interpolation = interpolation;
        return true;
    }

    if (propertyName == "unit") {
        std::string lowerValue = ToLower(valueText);

        if (lowerValue == "degrees" || lowerValue == "degree" || lowerValue == "deg") {
            command->usesDegrees = true;
            return true;
        }
        if (lowerValue == "radians" || lowerValue == "radian" || lowerValue == "rad") {
            command->usesDegrees = false;
            return true;
        }

        if (errorMessage != nullptr) {
            *errorMessage = "Unknown rotate unit: " + valueText;
        }
        return false;
    }

    if (propertyName == "key" || propertyName == "keyframe") {
        std::vector<float> values;

        if (!ParseFloatList(valueText, &values) || values.size() < 2) {
            if (errorMessage != nullptr) {
                *errorMessage = "Key requires time and at least one value.";
            }
            return false;
        }

        AnimationCommandKey key;
        key.timeSeconds = values[0];
        key.interpolation = command->interpolation;

        for (std::size_t valueIndex = 1; valueIndex < values.size(); ++valueIndex) {
            key.values.push_back(values[valueIndex]);
        }

        if (key.timeSeconds < 0.0f) {
            key.timeSeconds = 0.0f;
        }

        command->keys.push_back(key);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unknown command property: " + propertyName;
    }

    return false;
}

bool AnimationCommandParser::ParseFloatList(const std::string& text, std::vector<float>* values) const
{
    if (values == nullptr) {
        return false;
    }

    values->clear();
    std::string normalizedText = text;

    for (std::size_t charIndex = 0; charIndex < normalizedText.size(); ++charIndex) {
        char currentChar = normalizedText[charIndex];

        if (currentChar == ',') {
            normalizedText[charIndex] = ' ';
        }
    }

    std::istringstream stream(normalizedText);
    std::string token;

    while (stream >> token) {
        char* endPointer = nullptr;
        float value = std::strtof(token.c_str(), &endPointer);

        if (endPointer == token.c_str()) {
            return false;
        }

        values->push_back(value);
    }

    return !values->empty();
}

bool AnimationCommandParser::ParseInterpolation(const std::string& text, UIEditorInterpolation* interpolation) const
{
    if (interpolation == nullptr) {
        return false;
    }

    std::string lowerText = ToLower(Trim(text));

    if (lowerText == "linear") {
        *interpolation = UIEditorInterpolation::Linear;
        return true;
    }
    if (lowerText == "easein" || lowerText == "inquad" || lowerText == "in") {
        *interpolation = UIEditorInterpolation::EaseIn;
        return true;
    }
    if (lowerText == "easeout" || lowerText == "outquad" || lowerText == "out") {
        *interpolation = UIEditorInterpolation::EaseOut;
        return true;
    }
    if (lowerText == "easeinout" || lowerText == "inoutquad" || lowerText == "inout") {
        *interpolation = UIEditorInterpolation::EaseInOut;
        return true;
    }

    return false;
}

std::string AnimationCommandParser::Trim(const std::string& text) const
{
    std::size_t startIndex = 0;

    while (startIndex < text.size()) {
        unsigned char currentChar = static_cast<unsigned char>(text[startIndex]);

        if (!std::isspace(currentChar)) {
            break;
        }

        ++startIndex;
    }

    std::size_t endIndex = text.size();

    while (endIndex > startIndex) {
        unsigned char currentChar = static_cast<unsigned char>(text[endIndex - 1]);

        if (!std::isspace(currentChar)) {
            break;
        }

        --endIndex;
    }

    return text.substr(startIndex, endIndex - startIndex);
}

std::string AnimationCommandParser::ToLower(const std::string& text) const
{
    std::string lowerText = text;

    for (std::size_t charIndex = 0; charIndex < lowerText.size(); ++charIndex) {
        unsigned char currentChar = static_cast<unsigned char>(lowerText[charIndex]);
        lowerText[charIndex] = static_cast<char>(std::tolower(currentChar));
    }

    return lowerText;
}

std::string AnimationCommandParser::RemoveCommandNoise(const std::string& text) const
{
    std::string cleanedText = Trim(text);

    if (cleanedText.empty()) {
        return cleanedText;
    }

    if (cleanedText == "```") {
        return "";
    }

    if (cleanedText.rfind("```", 0) == 0) {
        return "";
    }

    if (cleanedText.rfind("//", 0) == 0) {
        return "";
    }

    if (cleanedText[0] == '#') {
        return "";
    }

    if (cleanedText[0] == '-' || cleanedText[0] == '*') {
        cleanedText = Trim(cleanedText.substr(1));
    }

    return cleanedText;
}
