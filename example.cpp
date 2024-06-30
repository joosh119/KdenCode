#include <iostream>
#include <string>
#include <vector>
#include "lib\KdenliveProject.h"

using namespace std;


/** COMPILE:
 *  g++ *.cpp lib/*.cpp -g -o example.exe
 *  
 *  RUN:
 *  example.exe
 */

// SET THESE TO RUN ON YOUR PC
/* const string MEDIA_FOLDER_PATH = "PATH_TO_EXAMPLE_MEDIA_FOLDER"; //*/
/* const string OUTPUT_FOLDER_PATH = "PATH_YOU_WANT_THE_PROJECT_FILE_TO_BE"; //*/


int main(int argc, char** argv){
    // Create project with 60 fps and 1080p resolution.
    KdenliveProject proj( 60, 1920, 1080 );

    // Add a video to the video track.
    proj.CreateClipOnVideoTrack( 0, "great_expanse", 10 );

    // Add some audio to the audio track.
    proj.CreateClipOnAudioTrack( 0, "Free_Test_Data_500KB_MP3", 20 );

    // Add a clip to both the audio and video tracks
    Clip clip_1 = proj.CreateClip("cavern_clinger_boss", 10);
    proj.AddClipToVideoTrack(9, clip_1);
    proj.AddClipToAudioTrack(9, clip_1);

    // You can set the length and fade of the clip after adding it
    clip_1.SetBounds(15);
    clip_1.SetFadeOffsets(1, 0);

    // Generate the .kdenlive file. The resulting file should open in Kdenlive.
    vector<string> media_paths = {MEDIA_FOLDER_PATH};
    proj.SaveToFile( media_paths, OUTPUT_FOLDER_PATH, "example_generated_project" );

    return 0;
}