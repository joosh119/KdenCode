#include <iomanip>
#include <sstream>
#include <cmath>
#include "KdenliveFile.h"

using namespace std;
using namespace tinyxml2;


// This file is simply the default file created by Kdenlive when creating a new project.
// The only changes that were made to the file were to remove references to filepaths.
// Using this library to modify a file that has already been edited in will not work, as Kdenlive generates a lot of data that we don't generate here.
const char* EMPTY_PROJECT_FILEPATH = "dependencies/empty_project.kdenlive";


ifstream openInputFile(const string &file_path){
	//open file
	ifstream input_file(file_path);

	//check if file was opened successfully
	if(!input_file.good()){
		cerr << "File '" << file_path << "' not found";
		exit(-1);
	}

	return input_file;
}

ofstream openOutputFile(const string &filePath){
	//open file
	ofstream output_file(filePath);

	return output_file;
}

string readEntireFile(ifstream &input_file){
	return string(istreambuf_iterator<char>(input_file), istreambuf_iterator<char>());
}

string convertToTimestamp(float seconds){
    // Calculate hours, minutes, seconds, and milliseconds
    int hours = static_cast<int>(seconds) / 3600;
    seconds = fmod(seconds, 3600);
    int minutes = static_cast<int>(seconds) / 60;
    seconds = fmod(seconds, 60);
    int secs = static_cast<int>(seconds);
    int milliseconds = static_cast<int>((seconds - secs) * 1000);
    
    // Use stringstream for formatting the string
    stringstream ss;
    ss << setw(2) << setfill('0') << hours << ":"
       << setw(2) << setfill('0') << minutes << ":"
       << setw(2) << setfill('0') << secs << "."
       << setw(3) << setfill('0') << milliseconds;
    
    return ss.str();
}


// CONSTRUCTORS
KdenliveFile::KdenliveFile(){
    // Initialize the counts of certain elements in the empty file
    chain_count = 0;
    track_count = 0;
    filter_count = 0;
    track_lengths = vector<float>();
    track_entries = vector<vector<TrackEntry>>();
    
    // Get "empty" kdenlive file a string and parse it
    {
    ifstream input_file = openInputFile(EMPTY_PROJECT_FILEPATH);
    static const string empty_project_string = readEntireFile(input_file);
    input_file.close(); 

    xml_doc.Parse( empty_project_string.c_str() );
    }

    // Set the root
    root = xml_doc.RootElement();

    // Set the profile and main producer
    profile = root->FirstChildElement();
    main_producer = profile->NextSiblingElement();

    // Find and set the main bin
    main_bin = FindPlaylistElement("main_bin");

    // Find the timeline tractor, which holds every track
    string timeline_tractor_id = FindDocUUID();
    timeline_tractor = FindTractorElement(timeline_tractor_id.c_str());

    // Set the final tractor
    final_tractor = main_bin->NextSiblingElement();
    final_tractor->SetAttribute("id", "final_tractor");

    // The element we want to start adding after is the producer
    last_added_root_element = main_producer;


    // Delete all the tracks the Kdenlive pre-generate in new files
    // We do this so we don't have to manually modify the generated empty file to get a file with 0 tracks
    DeletePreExistingTracks();
}


// SETTERS
void KdenliveFile::SetProfile(const int framerate, const int width, const int height){
    // Set profile values
    profile->SetAttribute("frame_rate_den", 1);         // We're just using a framerate density of 1
    profile->SetAttribute("frame_rate_num", framerate);
    profile->SetAttribute("width", width);
    profile->SetAttribute("height", height);

    // Set description of new profile
    string description =  to_string(width) + "x" + to_string(height) + ", " + to_string(framerate) + " fps";
    profile->SetAttribute("description", description.c_str());

    // Remove kdenlive:docproperties.profile property in main bin, so that these changes apply
    XMLElement* profile_property = main_bin->FirstChildElement();
    while(!profile_property->Attribute("name", "kdenlive:docproperties.profile")){
        profile_property = profile_property->NextSiblingElement();
        if(profile_property == nullptr)
            break;
    }
    main_bin->DeleteChild(profile_property);
}

TrackId KdenliveFile::AddTrack(const TrackType track_type){
    // Add two playlists
    int playlist_index_1 = (track_count) * 2;
    int playlist_index_2 = playlist_index_1 + 1;
    string playlist_str_1 = "playlist" + to_string(playlist_index_1);
    string playlist_str_2 = "playlist" + to_string(playlist_index_2);

    XMLElement* playlist_1 = CreatePlaylistElement(playlist_str_1.c_str());
    AddElementToRoot(playlist_1);
    XMLElement* playlist_2 = CreatePlaylistElement(playlist_str_2.c_str());
    AddElementToRoot(playlist_2);

    // Add a tractor
    string tractor_str = "tractor" + to_string(track_count);
    XMLElement* tractor = CreateTractorElement(tractor_str.c_str());
    AddElementToRoot(tractor);

    // Add playlists to tractor as tracks
    XMLElement* track_1 = AddTrackElement(tractor, playlist_str_1.c_str());
    XMLElement* track_2 = AddTrackElement(tractor, playlist_str_2.c_str());

    // Set track type specific propertes
    if(track_type == TrackType::VIDEO){ // VIDEO
        track_1->SetAttribute("hide", "audio");
        track_2->SetAttribute("hide", "audio");
    }

    else if(track_type == TrackType::AUDIO){ // AUDIO
        track_1->SetAttribute("hide", "video");
        track_2->SetAttribute("hide", "video");
        // Create the audio property to be added to the playlists and tractor
        XMLElement* audio_property = CreatePropertyElement("kdenlive:audio_track", "1");
        playlist_1->InsertFirstChild(audio_property);
        playlist_2->InsertFirstChild(audio_property);
        tractor->InsertFirstChild(audio_property);

    }

    // Add tractor to timeline as a track
    AddTrackElement(timeline_tractor, tractor_str.c_str());

    // Set internal data
    track_count ++;
    track_lengths.push_back(0);
    track_entries.push_back( vector<TrackEntry>() );

    return track_count - 1;
}

ClipId KdenliveFile::AddClipToBin(const std::string &clip_path){
    // Create chain
    const char* chain_name = ("chain" + to_string(chain_count)).c_str();
    XMLElement* chain =  CreateChainElement(chain_name, clip_path.c_str());
    
    // Add chain above all playlists and tractors
    AddElementToTopOfRoot(chain);

    // Add entry to main bin
    AddEntryElement(main_bin, 0, 0, chain_name);

    // Set internal data
    chain_count++;

    return chain_count-1;
}

TrackEntryId KdenliveFile::AddBlankToTrack(const TrackId track_id, const float length){
    // Find the playlist to add to. Since there are two playlist "tracks" for every track, we will just set to the first even one
    int playlist_index = track_id * 2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* track_playlist = FindPlaylistElement(playlist_id.c_str());

    // Add blank entry
    AddBlankElement(track_playlist, length);

    // Create TrackEntry
    TrackEntry entry;
    entry.entry_type = EntryType::BLANK;
    entry.length = length;
    entry.start_offset = 0;

    // Set internal data
    track_lengths[track_id] += length;
    track_entries[track_id].push_back(entry);

    return track_entries[track_id].size() - 1;
}

TrackEntryId KdenliveFile::AddClipToTrack(const TrackId track_id, const ClipId clip_id, const float clip_length, const float clip_start_offset){
    // Find the playlist to add to. Since there are two playlist "tracks" for every track, we will just set to the first even one
    int playlist_index = track_id * 2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* track_playlist = FindPlaylistElement(playlist_id.c_str());

    // Add entry
    string chain_str = "chain" + to_string(clip_id);
    AddEntryElement(track_playlist, clip_start_offset, clip_length + clip_start_offset, chain_str.c_str());

    // Create TrackEntry
    TrackEntry entry;
    entry.entry_type = EntryType::CLIP;
    entry.length = clip_length;
    entry.start_offset = clip_start_offset;

    // Set internal data
    track_lengths[track_id] += clip_length;
    track_entries[track_id].push_back(entry);

    return  track_entries[track_id].size() - 1;
}

void KdenliveFile::FadeClip(const TrackId track_id, TrackEntryId entry_id, const float fade_in_time, const float fade_out_time){
    // Get this TrackEntry
    const TrackEntry this_entry = track_entries[track_id][entry_id];

    // Don't allow a filter to be applied to Blank entries
    if(this_entry.entry_type == EntryType::BLANK)
        return;

    // Get the entry in the doc
    const int playlist_index = track_id*2;
    const string playlist_id = "playlist" + to_string(playlist_index);
    XMLElement* entry = FindPlaylistEntry(playlist_id.c_str(), entry_id);
    

    // Fade in
    if(fade_in_time > 0){
        const string filter_id = "filter" + to_string(filter_count);
        XMLElement* filter = CreateFilterElement(filter_id.c_str(), this_entry.start_offset, this_entry.start_offset + fade_in_time);
        AddPropertyElement(filter, "start", "1");
        AddPropertyElement(filter, "level", "1");
        AddPropertyElement(filter, "mlt_service", "brightness");
        AddPropertyElement(filter, "kdenlive_id", "fade_from_black");
        AddPropertyElement(filter, "alpha", "0=0;-1=1");
        entry->InsertEndChild(filter);

        filter_count++;
    }
    // Fade out
    if(fade_out_time > 0){
        const string filter_id = "filter" + to_string(filter_count);
        XMLElement* filter = CreateFilterElement(filter_id.c_str(), this_entry.start_offset + this_entry.length - fade_out_time, this_entry.start_offset + this_entry.length);
        AddPropertyElement(filter, "start", "1");
        AddPropertyElement(filter, "level", "1");
        AddPropertyElement(filter, "mlt_service", "brightness");
        AddPropertyElement(filter, "kdenlive_id", "fade_to_black");
        AddPropertyElement(filter, "alpha", "0=1;-1=0");
        entry->InsertEndChild(filter);

        filter_count++;
    }
    

    
}


// GETTERS
float KdenliveFile::GetTrackLength(const TrackId track_id){
    return track_lengths[track_id];
}

string KdenliveFile::ToString() const{
    XMLPrinter printer;
    xml_doc.Print(&printer);

    string xml_string = printer.CStr();
    return xml_string;
}

void KdenliveFile::SaveToFile(const string &file_name, const string &output_filepath) const{
    ofstream output;

    if(output_filepath != "")
        output = openOutputFile(output_filepath + "/" + file_name + ".kdenlive");
    else
        output = openOutputFile(file_name + ".kdenlive");
	
    output << ToString();

	output.close();
}


// HELPERS
XMLElement* KdenliveFile::CreatePropertyElement(const char* name, const char* value){
    XMLElement* property = xml_doc.NewElement("property");
    property->SetAttribute("name", name);
    property->SetText(value);

    return property;
}
XMLElement* KdenliveFile::AddPropertyElement(XMLElement* element_to_add_to, const char* name, const char* value){
    XMLElement* property = CreatePropertyElement(name, value);
    element_to_add_to->InsertEndChild(property);

    return property;
}

XMLElement* KdenliveFile::CreateEntryElement(const float in, const float out, const char* producer){
    XMLElement* entry = xml_doc.NewElement("entry");
    const char* in_str = convertToTimestamp(in).c_str();
    const char* out_str = convertToTimestamp(out).c_str();

    entry->SetAttribute("in", in_str);
    entry->SetAttribute("out", out_str);
    entry->SetAttribute("producer", producer);

    return entry;
}
XMLElement* KdenliveFile::AddEntryElement(XMLElement* element_to_add_to, const float in, const float out, const char* producer){
    XMLElement* entry = CreateEntryElement(in, out, producer);
    element_to_add_to->InsertEndChild(entry);

    return entry;
}

XMLElement* KdenliveFile::CreateBlankElement(const float length){
    XMLElement* blank = xml_doc.NewElement("blank");
    const string length_str = convertToTimestamp(length);

    blank->SetAttribute("length", length_str.c_str());

    return blank;
}
XMLElement* KdenliveFile::AddBlankElement(XMLElement* element_to_add_to, const float length){
    XMLElement* blank = CreateBlankElement(length);
    element_to_add_to->InsertEndChild(blank);

    return blank;
}

XMLElement* KdenliveFile::CreateTrackElement(const char* producer){
    XMLElement* track = xml_doc.NewElement("track");

    track->SetAttribute("producer", producer);

    return track;
}
XMLElement* KdenliveFile::AddTrackElement(XMLElement* element_to_add_to, const char* producer){
    XMLElement* track = CreateTrackElement(producer);
    element_to_add_to->InsertEndChild(track);

    return track;
}

XMLElement* KdenliveFile::CreateFilterElement(const char* id, const float in, const float out){
    XMLElement* filter = xml_doc.NewElement("filter");
    const char* in_str = convertToTimestamp(in).c_str();
    const char* out_str = convertToTimestamp(out).c_str();

    filter->SetAttribute("id", id);
    filter->SetAttribute("in", in_str);
    filter->SetAttribute("out", out_str);

    return filter;
}
XMLElement* KdenliveFile::AddFilterElement(XMLElement* element_to_add_to, const char* id, const float in, const float out){
    XMLElement* filter = CreateFilterElement(id, in, out);
    element_to_add_to->InsertEndChild(filter);

    return filter;
}

XMLElement* KdenliveFile::CreateChainElement(const char* id, const char* resource){
    XMLElement* chain = xml_doc.NewElement("chain");

    chain->SetAttribute("id", id);

    // Add resource as a property
    AddPropertyElement(chain, "resource", resource);

    return chain;
}
XMLElement* KdenliveFile::AddChainElement(XMLElement* element_to_add_to, const char* id, const char* resource, XMLElement* insert_after){
    XMLElement* chain = CreateChainElement(id, resource);
    
    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(chain);
    else
        element_to_add_to->InsertAfterChild(insert_after, chain);

    return chain;
}

XMLElement* KdenliveFile::CreatePlaylistElement(const char* id){
    XMLElement* playlist = xml_doc.NewElement("playlist");

    playlist->SetAttribute("id", id);

    return playlist;
}
XMLElement* KdenliveFile::AddPlaylistElement(XMLElement* element_to_add_to, const char* id, XMLElement* insert_after){
    XMLElement* playlist = CreatePlaylistElement(id);
    
    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(playlist);
    else
        element_to_add_to->InsertAfterChild(insert_after, playlist);

    return playlist;
}

XMLElement* KdenliveFile::CreateTractorElement(const char* id){
    XMLElement* tractor = xml_doc.NewElement("tractor");

    tractor->SetAttribute("id", id);

    return tractor;
}
XMLElement* KdenliveFile::AddTractorElement(XMLElement* element_to_add_to, const char* id, XMLElement* insert_after){
    XMLElement* tractor = CreateTractorElement(id);

    if(insert_after == nullptr)
        element_to_add_to->InsertEndChild(tractor);
    else
        element_to_add_to->InsertAfterChild(insert_after, tractor);

    return tractor;
}

// All elements added to the root (playlists and tractors) must be added after the main_producer, but before the main_bin.
void KdenliveFile::AddElementToTopOfRoot(XMLElement* element){
    // If nothing has been added to the root yet, then set this element as the last added element
    if(last_added_root_element == main_producer){
        last_added_root_element = element;
    }

    // Add after the main_producer
    root->InsertAfterChild(main_producer, element);
}
void KdenliveFile::AddElementToRoot(XMLElement* element){
    root->InsertAfterChild(last_added_root_element, element);
    
    // Set this as the last added root
    last_added_root_element = element;
}

XMLElement* KdenliveFile::FindPlaylistElement(const char* playlist_id) const{
    XMLElement* ptr = root->FirstChildElement();
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "playlist") == 0  &&  ptr->Attribute("id", playlist_id) ){
            break;
        }

        ptr = ptr->NextSiblingElement();
    }

    return ptr;
}

XMLElement* KdenliveFile::FindTractorElement(const char* tractor_id) const{
    XMLElement* ptr = root->FirstChildElement();
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "tractor") == 0  &&  ptr->Attribute("id", tractor_id) ){
            break;
        }

        ptr = ptr->NextSiblingElement();
    }

    return ptr;
}

XMLElement* KdenliveFile::FindPlaylistEntry(const char* playlist_id, const TrackEntryId entry_index){
    // Find playlist
    XMLElement* playlist = FindPlaylistElement(playlist_id);
    
    // Count through the entries of the playlist
    XMLElement* ptr = playlist->FirstChildElement();
    int c = 0;
    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "entry") == 0  ||  strcmp(ptr->Name(), "blank") == 0){
            c++;

            if( c > entry_index)    break;
        }

        ptr = ptr->NextSiblingElement();
    }
    
    return ptr;
}

string KdenliveFile::FindDocUUID(){
    // Check main bin for kdenlive:docproperties.uuid property
    XMLElement* ptr = main_bin->FirstChildElement();

    while(ptr != nullptr){
        if( strcmp(ptr->Name(), "property") == 0  &&  ptr->Attribute("name", "kdenlive:docproperties.uuid") )
            break;
        
        ptr = ptr->NextSiblingElement();
    }

    if(ptr == nullptr)
        return "NULL_UUID";

    return ptr->GetText();
}

void KdenliveFile::DeletePreExistingTracks(){
    // Delete all playlists and tractors up to the timeline_tractor
    XMLElement* ptr = main_producer->NextSiblingElement();
    while(ptr != timeline_tractor){
        XMLElement* cur_ptr = ptr;
        ptr = ptr->NextSiblingElement();

        if(strcmp(cur_ptr->Name(), "playlist") == 0){
            root->DeleteChild(cur_ptr);
        }
        else if(strcmp(cur_ptr->Name(), "tractor") == 0){
            const char* tractor_id = cur_ptr->Attribute("id");
            root->DeleteChild(cur_ptr);

            // Delete tractor id from timeline tractor
            XMLElement* t_ptr = timeline_tractor->FirstChildElement();
            while(t_ptr != nullptr){
                if(t_ptr->Attribute("producer", tractor_id)){
                    timeline_tractor->DeleteChild(t_ptr);
                    break;
                }

                t_ptr = t_ptr->NextSiblingElement();
            }
        }
    }
}
