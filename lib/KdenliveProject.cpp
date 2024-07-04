#include <filesystem>
#include "KdenliveProject.h"

using namespace std;
namespace fs = std::filesystem;
using namespace tinyxml2;


const char* DEFAULT_MEDIA_FORMAT = ".mp4";


string findFilePath(const vector<string> &media_folder_paths, const string &file_name){
    // Iterate through each folder
	for( const string &folder_path : media_folder_paths ){
		// Check if file exists
		if( !std::ifstream(folder_path).good() )
			continue;

		for( const auto &entry : fs::directory_iterator(folder_path) ) {
			// Check that the entry is not a folder
			if( entry.is_regular_file() ) {	
				// Check if the file name matches
				if( entry.path().stem().string() == file_name ) {
					return entry.path().string();
				}
			}
		}
	}

    // Return the filename with the default media format extension at the end if the file wasn't found
    return file_name + DEFAULT_MEDIA_FORMAT;
}


// Clip --------------------------------------------------
Clip::Clip(const string &name, const float length, const float start_offset){
	this->name = name;
	this->length = length;
	this->start_offset = start_offset;
}

void Clip::SetBounds(const float length, const float start_offset){
	if(length > 0)
		this->length = length;
	if(start_offset > 0)
		this->start_offset = start_offset;
}

void Clip::SetFadeOffsets(const float fade_in_time, const float fade_out_time){
	this->fade_in_time = fade_in_time;
	this->fade_out_time = fade_out_time;
}


// KdenliveProject --------------------------------------------------
// CONSTRUCTORS
KdenliveProject::KdenliveProject(){
	this->framerate = 30;
	this->frame_width = 1920;
	this->frame_height = 1080;
}


// SETTERS
void KdenliveProject::SetProfile(const float framerate, const int frame_width, const int frame_height){
	if(framerate > 0)
		this->framerate = framerate;
	if(frame_width > 0)
		this->frame_width = frame_width;
	if(frame_height > 0)
		this->frame_height = frame_height;
}

Clip* KdenliveProject::CreateClip(const string &name, const float length, const float start_offset){
	clips.push_back(Clip(name, length, start_offset));
	return &clips.back();
}

void KdenliveProject::AddClipToVideoTrack(const float time_stamp, Clip* clip){
	video_timeline.insert( { time_stamp, clip} );
}
void KdenliveProject::AddClipToAudioTrack(const float time_stamp, Clip* clip){
	audio_timeline.insert( { time_stamp, clip} );
}

Clip* KdenliveProject::CreateClipOnVideoTrack(const float time_stamp, const string &name, const float length, const float start_offset){
	Clip* new_clip = CreateClip(name, length, start_offset);

	AddClipToVideoTrack(time_stamp, new_clip);

	return new_clip;
}
Clip* KdenliveProject::CreateClipOnAudioTrack(const float time_stamp, const string &name, const float length, const float start_offset){
	Clip* new_clip = CreateClip(name, length, start_offset);

	AddClipToAudioTrack(time_stamp, new_clip);

	return new_clip;
}
	

// GENERATE PROJECT FILE
KdenliveFile* KdenliveProject::GenerateFile(const vector<string> &media_folder_paths){ // Kind of a mess, needs reworking
	// Reset the file
	//kdenlive_file.~KdenliveFile();	// Destructor doesn't do anything currently
	//new (&kdenlive_file) KdenliveFile();
	
	KdenliveFile* kdenlive_file = new KdenliveFile;
	
	// Start the document
	kdenlive_file->SetProfile(framerate, frame_width, frame_height);
	
	// Add all filepaths to the KdenliveFile file
	map<string, ClipId> included_names = map<string, ClipId>();
	
	for(const Clip clip : clips){
		
		// Check if the clip has not been to the file
		if(included_names.find(clip.name) == included_names.end()){
			// Find the filepath to use for this clip
			const string filepath = findFilePath(media_folder_paths, clip.name);
			// Add the clip the the KdenliveFile bin
			ClipId clip_id = kdenlive_file->AddClipToBin(filepath);
			
			// Add clip_id to included_names map
			included_names.insert( {clip.name, clip_id} );
		}
	}
	
	// Macro that adds a clip to a track
	#define ADD_CLIP	float blank_length = entry_start_time - track_length;														\
						if( blank_length > 0.00099)																					\
						kdenlive_file->AddBlankToTrack(track_id, blank_length);														\
						ClipId clip_id = included_names.at(clip->name);																\
						TrackEntryId entry_id = kdenlive_file->AddClipToTrack(track_id, clip_id, clip->length, clip->start_offset);	\
						kdenlive_file->FadeClip(track_id, entry_id, clip->fade_in_time, clip->fade_out_time);						\
	
	// Add video clips
	vector<TrackId> video_tracks;
	
	for(auto video : video_timeline){
		const float entry_start_time = video.first;
		const Clip* clip = video.second;
		
		// Check for availible tracks from the bottom up
		for(int i = 0; i < video_tracks.size(); i++){
			TrackId track_id = video_tracks[i];
			float track_length = kdenlive_file->GetTrackLength(track_id);
		
			// Check if clip can fit on track
			if( track_length <= entry_start_time ){
				ADD_CLIP
				goto video_continue;
			}
		}
		{
		// If there was no availible track, create a new one and add it there
		TrackId track_id = kdenlive_file->AddTrack(KdenliveFile::VIDEO);		
		video_tracks.push_back(track_id);
		float track_length = 0;
		ADD_CLIP
		}

		video_continue: ;
	}
	
	// Add audio clips
	vector<TrackId> audio_tracks;
	
	for(auto audio : audio_timeline){
		float entry_start_time = audio.first;
		Clip* clip = audio.second;
		// Check for availible tracks from the bottom up
		for(int i = 0; i < audio_tracks.size(); i++){
			TrackId track_id = audio_tracks[i];
			float track_length = kdenlive_file->GetTrackLength(track_id);

			// Check if clip can fit on track AND has
			if( track_length <= entry_start_time ){
				ADD_CLIP
				goto audio_continue;
			}
		}
		{
		// If there was no availible track, create a new one and add it there
		TrackId track_id = kdenlive_file->AddTrack(KdenliveFile::AUDIO);
		audio_tracks.push_back(track_id);
		float track_length = 0;
		ADD_CLIP
		}
		
		audio_continue: ;
	}
	
	return kdenlive_file;
}

string KdenliveProject::SaveAsString(const vector<string> &media_folder_paths){
	// Generate the file
	KdenliveFile* file = GenerateFile(media_folder_paths);

	const string file_str = file->ToString();

	// Deallocate the file
	delete file;

	return file_str;
}

void KdenliveProject::SaveToFile(const vector<string> &media_folder_paths, const string &file_name, const string &output_filepath){
	// Generate the file
	KdenliveFile* file = GenerateFile(media_folder_paths);
	
	file->SaveToFile(file_name, output_filepath);

	// Deallocate the file
	delete file;
}
