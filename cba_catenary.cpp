#include "stdafx.h"

#include <string>
#include <vector>
#include <cmath>

#define VERSION "1.0.2"

#define MAX_ITERATIONS_BISECTION 20
#define MAX_ITERATIONS_NEWTON 10
#define MAX_DIFF_BISECTION 0.01
#define MAX_DIFF_NEWTON 0.001
#define MAX_SEGMENT_COUNT 300
#define MAX_SEGMENT_DIFF 0.01

#define SYNTAX_ERROR_WRONG_PARAMS_SIZE 101
#define SYNTAX_ERROR_WRONG_PARAMS_TYPE 102
#define PARAMS_ERROR_TOO_MANY_ARGS 201
#define EXECUTION_WARNING_TAKES_TOO_LONG 301

#define EXTENSION_RETURN 0
#define EXTENSION_RETURN_SAME_POSITION 100
#define EXTENSION_RETURN_ROPE_TO_SHORT 200
#define EXTENSION_RETURN_TENSION_TOO_HIGH 300

extern "C"
{
    __declspec(dllexport) void __stdcall RVExtensionVersion(char *output, int outputSize);
    __declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
    __declspec(dllexport) int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt);
}

void __stdcall RVExtensionVersion(char *output, int outputSize)
{
    strncpy_s(output, outputSize, VERSION, _TRUNCATE);
}

void __stdcall RVExtension(char *output, int outputSize, const char *function)
{
    std::string result;

    if (strcmp(function, "version") == 0)
        result = VERSION;
    else if (strcmp(function, "draw") == 0)
        result = "[]";

    strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
}

/* delimiter positions */
void params_array3d(float pos[3], std::string posstr)
{
    posstr.erase(0, 1);
    posstr.erase(posstr.length() - 1, 1);
    int delim1 = posstr.find(",");
    int delim2 = posstr.find(",", delim1 + 1);

    pos[0] = stof(posstr.substr(0, delim1));
    pos[1] = stof(posstr.substr(delim1 + 1, delim2 - delim1 - 1));
    pos[2] = stof(posstr.substr(delim2 + 1));
};

int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt)
{
    std::string result = "[]";

    if (strcmp(function, "version") == 0)
        result = VERSION;
    else
    {
        float pos1[3]{ 0,0,0 };
        float pos2[3]{ 0,0,0 };
        float rope_length = 0;
        int segment_count = 0;
        float segment_length = -1;

        /*if (argsCnt < 4) {
        strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
        return SYNTAX_ERROR_WRONG_PARAMS_SIZE;
        }*/

        if (argsCnt > 0)
            params_array3d(pos1, args[0]);

        if (argsCnt > 1)
            params_array3d(pos2, args[1]);

        if (argsCnt > 2)
            rope_length = atof(args[2]);

        if (argsCnt > 3)
            segment_count = atof(args[3]);

        if (argsCnt > 4)
            segment_length = atof(args[4]);

        /* calculate the 2d catenary curve */
        float delta_x = sqrt(pow(pos2[0] - pos1[0], 2) + pow(pos2[1] - pos1[1], 2));
        float delta_y = pos2[2] - pos1[2];

        if (delta_x == 0) {
            strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
            return EXTENSION_RETURN_SAME_POSITION;
        }

        if (rope_length <= delta_x) {
            strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
            return EXTENSION_RETURN_ROPE_TO_SHORT;
        }

        float a = 1.0;
        float aMin = 0.1;
        float aMax = 10 * rope_length;
        float diff = 1.0e+7;
        float alpha = sqrt(pow(rope_length, 2) - pow(delta_y, 2));

        for (int i = 1; i <= MAX_ITERATIONS_BISECTION && abs(diff) > MAX_DIFF_BISECTION; i++) {
            a = (aMin + aMax) / 2;
            diff = 2 * a * sinh(delta_x / (2 * a)) - alpha;

            if (diff < 0)
                aMax = a;
            else
                aMin = a;
        }

        float prev = 1.0e+7;

        for (int i = 1; i <= MAX_ITERATIONS_NEWTON && abs(prev - a) > MAX_DIFF_NEWTON; i++) {
            a = a -
                (2 * a * sinh(delta_x / (2 * a)) - alpha) /
                (2 * sinh(delta_x / (2 * a)) - delta_x / a * cosh(delta_x / (2 * a)));

            prev = a;
        }

        if (!isfinite(a)) {
            strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
            return EXTENSION_RETURN_TENSION_TOO_HIGH;
        }

        float x1 = a * atanh(delta_y / rope_length) - delta_x / 2;
        float x2 = a * atanh(delta_y / rope_length) + delta_x / 2;
        float y1 = a * cosh(x1 / a);
        float y2 = a * cosh(x2 / a);

        /* estimate amount of needed segments and increase segment length if necessary */
        if (segment_count > MAX_SEGMENT_COUNT)
            segment_count = MAX_SEGMENT_COUNT;

        if (segment_length < 0)
            segment_length = rope_length / segment_count;
        else
            segment_count = ceil(rope_length / segment_length);

        /* generate vector of x,y points on catenary */
        std::vector<float> catenary_points;
        catenary_points.reserve(2 * segment_count + 2);

        float s = a * sinh(x1 / a);

        for (int i = 0; i <= segment_count; i++) {
            float x = a * asinh(s / a);
            float y = a * cosh(x / a);
            s = s + segment_length;

            catenary_points.push_back(x);
            catenary_points.push_back(y);
        }

        /* convert 2d points to world positions */
        float dir_map[2]{ pos2[0] - pos1[0], pos2[1] - pos1[1] };
        float dir_map_length = sqrt(dir_map[0] * dir_map[0] + dir_map[1] * dir_map[1]);
        dir_map[0] = dir_map[0] / dir_map_length;
        dir_map[1] = dir_map[1] / dir_map_length;

        result.reserve(catenary_points.size() * 8);
        result = "[";

        char buffer[256];
        for (unsigned i = 0; i < catenary_points.size(); i = i + 2) {
            auto add = _snprintf_s(buffer, _TRUNCATE, "[%f,%f,%f],",
                pos1[0] + (catenary_points.at(i) - x1) * dir_map[0],
                pos1[1] + (catenary_points.at(i) - x1) * dir_map[1],
                pos1[2] + catenary_points.at(i + 1) - y1
            );
            result.append(buffer, add);
        }

        result.erase(result.length() - 1, 1);
        result = result + "]";
    }

    strncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
    return 0;
}
