#ifndef KDENLIVEPROJECT_H
#define KDENLIVEPROJECT_H


#include <string>
#include <list>
#include <map>
#include "KdenliveFile.h"


class KdenliveProject;

// Class for managing clips
class Clip{
	friend KdenliveProject;

	public:
	/** Sets the bounds of the clip.
	 *	Neither the length nor start offset can be non-positive.
	 *	@param length specifies how long the clip will be on the track.
	 *	@param start_offset specified how far from the beginning of the clip that the clip will begin playing on the track.
	*/
	void SetBounds(const float length, const float start_offset = 0);
	/** Sets the fade effect of the clip.
	 *	@param fade_in_time specifies how long the fade will last at the beginning of the entry.
	*	@param fade_out_time specifies how long the fade will last at the end of the entry.
	*/
	void SetFadeOffsets(const float fade_in_time, const float fade_out_time);
	/**	Sets the visibility priority of a video clip in the timeline.
	 * 	That is, if two video clips play at the same time, the clip with the higher priority will be visible.
	 * 	This only affects clip which are added to a video track.
	 */
	// void SetPriority(const int priority); // NOT IMPLEMENTED

	private:
	Clip(const std::string &name, const float length, const float start_offset = 0); // Clips should only be created from within the KdenliveProject
	
	std::string name;
	float length;
	float start_offset;
	float fade_in_time = 0;
	float fade_out_time = 0;
	int priority = 0;
};


class KdenliveProject{
	public:
	// CONSTRUCTORS
	/**	Creates a KdenliveProject with the default profile of 30 fps and 1080p resolution.
	 */
	KdenliveProject();
	
	// SETTERS
	/**	Sets the profile of the video.
	 * 	NOTE: Kdenlive may only allow certain profile presets, so the profile you specify here may be overwritten by Kdenlive. 
     * 
     *  @param framerate specifies the framerate of the video
     *  @param frame_width specifies the width, in pixels, of the video
     *  @param frame_height specifies the length, in pixels, of the video
	 */
	void SetProfile(const float framerate, const int frame_width, const int frame_height);
	/**	Creates a clip with the given name and length.
	 * 	This clip can then be passed to AddClipToVideoTrack() and/or AddClipToAudioTrack() to add it to the timeline.
	 * 	If you add the same Clip* multiple times to a track, then any changes made to the clip will be reflected across the entire timeline.
	 * 
	 * 	NOTE: The name of the clip should not contain the file extension, as the file name is searched for when generating the KdenliveFile.
	 * 	
	 * 	@param name specifies the name of the clip.
	 * 	@param length specifies how long the clip will be on the track.
	 *	@param start_offset specifies how far from the beginning of the clip that the clip will begin playing on the track.
	 */
	Clip* CreateClip(const std::string &name, const float length, const float start_offset = 0);
	/**	Adds a video clip at the given time.
	 * 
	 * 	@param time_stamp specifies the time that the clip starts at.
	 *	@param clip specifies the clip to be added to the track.
	 */
	void AddClipToVideoTrack(const float time_stamp, Clip* clip);
	/**	Adds an audio clip at the given time.
	 * 
	 * 	@param time_stamp specifies the time that the clip starts at.
	 *	@param clip specifies the clip to be added to the track.
	 */
	void AddClipToAudioTrack(const float time_stamp, Clip* clip);
	/**	Creates a clip with the given name and length, and then adds it to a video track at the given time.
	 * 	It is the same as calling both CreateClip() and AddClipToVideoTrack(), but in a more compact package.
	 * 
	 * 	@param time_stamp specifies the time that the clip starts at.
	 * 	@param name specifies the name of the clip.
	 * 	@param length specifies how long the clip will be on the track.
	 *	@param start_offset specifies how far from the beginning of the clip that the clip will begin playing on the track.
	 */
	Clip* CreateClipOnVideoTrack(const float time_stamp, const std::string &name, const float length, const float start_offset = 0);
	/**	Creates a clip with the given name and length, and then adds it to an audio track at the given time.
	 * 	It is the same as calling both CreateClip() and AddClipToAudioTrack(), but in a more compact package.
	 * 
	 * 	@param time_stamp specifies the time that the clip starts at.
	 * 	@param name specifies the name of the clip.
	 * 	@param length specifies how long the clip will be on the track.
	 *	@param start_offset specifies how far from the beginning of the clip that the clip will begin playing on the track.
	 */
	Clip* CreateClipOnAudioTrack(const float time_stamp, const std::string &name, const float length, const float start_offset = 0);

	// GENERATE PROJECT FILE
	/**	Generates a KdenliveFile and retrieves the string representing the file.
	 * 
	 * @param media_folder_paths is a collection of paths to folders that contain the media for the project.
	 */
	std::string SaveAsString(const std::vector<std::string> &media_folder_paths);
	/**	Generates a KdenliveFile and saves the file to the given path.
	 * 	This function appends ".kdenlive" to the file name automatically.
	 * 	If no output filepath is specified, then it will save the file to current directory.
	 * 
	 * 	@param media_folder_paths is a collection of paths to folders that contain the media for the project.
	 * 	@param output_filepath is the path you want to save the .kdenlive file to.
	 * 	@param file_name is the name you want to give the .kdenlive file.
	 */
	void SaveToFile(const std::vector<std::string> &media_folder_paths, 
					const std::string &file_name = "kdenlive_project",
					const std::string &output_filepath = "");
	
	
	private:
	KdenliveFile* GenerateFile(const std::vector<std::string> &media_folder_paths);

	// PRIVATE VARIABLES
	float framerate;
	int frame_width;
	int frame_height;
	std::list<Clip> clips;
	std::multimap<float, Clip*> video_timeline;
	std::multimap<float, Clip*> audio_timeline;
};


#endif