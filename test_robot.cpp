#include "RobotBase.h"
#include <iostream>
#include <vector>
#include <dlfcn.h>

RobotBase* load_robot(const std::string& shared_lib, void* &handle) 
{
    std::cout << "Testing robot from " << shared_lib << "...\n";

    // Dynamically load the shared library
    handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
    if (!handle) 
    {
        std::cerr << "Failed to load " << shared_lib << ": " << dlerror() << '\n';
        return nullptr;
    }

    // Locate the factory function to create the robot and 'assign' it to this 'create_robot' function.
    using RobotFactory = RobotBase* (*)();
    RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
    if (!create_robot) 
    {
        std::cerr << "Failed to find create_robot in " << shared_lib << ": " << dlerror() << '\n';
        dlclose(handle);
        return nullptr;
    }

    // Instantiate the robot - it will need to be deleted later. This actually calls the function that exists
    // in the ROBOT code! Cool huh! It's in the bottom of the Robot where it says extern "C"
    RobotBase* robot = create_robot();
    if (!robot) 
    {
        std::cerr << "Failed to create robot instance from " << shared_lib << '\n';
        dlclose(handle);
        return nullptr;
    }

    return robot;
}

// you can add or remove behaviors here if you like.
// make a custom version of this tester, so that it tests YOUR robot...
void test_robot_behavior(RobotBase* robot) 
{
    // Set up the robot
    robot->set_boundaries(20, 20);
    robot->move_to(10, 10); // Start in the middle of the arena

    // Print robot stats
    std::cout << "Robot Stats:" << std::endl;
    robot->print_stats();

    int inactive_turns = 0;

    for (int turn = 1; turn <= 10; ++turn) {
        std::cout << "\nSimulating turn " << turn << ":\n";

        bool took_action = false;

        // Simulate radar results
        std::vector<RadarObj> radar_results;
        int radar_direction = 0;

        // Call get_radar_direction and simulate radar scanning
        robot->get_radar_direction(radar_direction);
        std::cout << "Radar direction chosen: " << radar_direction << std::endl;

        if (turn == 2) {
            // Simulate detecting an enemy on turn 2
            RadarObj enemy('R', 10, 11);
            radar_results.push_back(enemy);
        }

        // Pass radar results to the robot
        robot->process_radar_results(radar_results);

        int shot_row = 0, shot_col = 0;
        if (robot->get_shot_location(shot_row, shot_col)) {
            std::cout << "Robot shoots at (" << shot_row << ", " << shot_col << ").\n";
            if (!radar_results.empty()) {
                const auto& enemy = radar_results[0];
                if (shot_row == enemy.m_row && shot_col == enemy.m_col) {
                    std::cout << "Shot location matches radar target: (" << enemy.m_row << ", " << enemy.m_col << ").\n";
                } else {
                    std::cerr << "Error: Shot location (" << shot_row << ", " << shot_col
                              << ") does not match radar target (" << enemy.m_row << ", " << enemy.m_col << ").\n";
                }
            }
            took_action = true;
        } else {
            std::cout << "No shooting this turn.\n";
        }

        // Simulate movement
        int move_direction = 0, move_distance = 0;
        robot->get_movement(move_direction, move_distance);

        if (move_direction != 0 && move_distance != 0) 
        {
            // Predefined directional increments for movement (1-8, clock directions) come from RobotBase
            int delta_row = directions[move_direction].first;
            int delta_col = directions[move_direction].second;

            // Calculate new position
            int current_row, current_col;
            robot->get_current_location(current_row, current_col);
            int target_row = std::clamp(current_row + delta_row * move_distance, 0, 19);
            int target_col = std::clamp(current_col + delta_col * move_distance, 0, 19);

            // Move the robot
            robot->move_to(target_row, target_col);

            std::cout << "Robot moves to (" << target_row << ", " << target_col << ").\n";

            // Verify the movement
            int verify_row, verify_col;
            robot->get_current_location(verify_row, verify_col);
            if (verify_row == target_row && verify_col == target_col) {
                std::cout << "Movement verified!\n";
            } else {
                std::cerr << "Error: Robot did not move to the expected location!\n";
            }

            took_action = true;
        } else {
            std::cout << "Robot chooses not to move.\n";
        }

        // Check for inactivity
        if (took_action) {
            inactive_turns = 0; // Reset inactivity counter
        } else {
            ++inactive_turns;
            if (inactive_turns >= 5) {
                std::cerr << "Warning: Robot has not moved or shot for " << inactive_turns << " consecutive turns.\n";
            }
        }
    }
}



int main(int argc, char* argv[]) 
{
    // we didn't talk about command line arguments. This is how they work. 'argc' contains
    // the number of arguments passed (including the name of the executable.)
    // 'argv[]' contains the array of strings that were passed in.
    if (argc != 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <robot_library>\n";
        return 1;
    }

    const std::string robot_file = argv[1];
    const std::string shared_lib = "lib" + robot_file.substr(0, robot_file.find(".cpp")) + ".so";

    // Compile the robot into a shared library -fPIC is Position Independant Code - look it up!
    std::string compile_cmd = "g++ -shared -fPIC -o " + shared_lib + " " + robot_file + " RobotBase.o -I. -std=c++20";
    std::cout << "Compiling " << robot_file << " into " << shared_lib << "...\n";

    if (std::system(compile_cmd.c_str()) != 0) {
        std::cerr << "Failed to compile " << robot_file << " into " << shared_lib << '\n';
        return 1;
    }

    RobotBase *robot;
    void *handle;

    robot = load_robot(shared_lib, handle);
    test_robot_behavior(robot);

    // Cleanup
    delete robot;
    dlclose(handle);

    std::cout << "Robot testing complete.\n";

    return 0;
}