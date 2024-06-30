# KdenCode
A small and simple C++ library for generating .kdenlive files programmatically.
The intended purpose of this library is not to be a robust editing alternative, but to help in semi-automating the process of placing large amounts of video and audio clips into the Kdenlive timeline.
Once a .kdenlive file has been generated, you can then open it in the Kdenlive video editor and then manually edit and/or render it.

# Features
- Setting the profile of the project file.
- Adding clips to the project bin.
- Adding new video and audio tracks to the timeline.
- Adding clips from the project bin onto a video or audio track, with a given position, length, and starting offset.
- Adding a fade effect to clips added to a track.

There are obviously many other effects/features that could be implemented later, but I see these as the bare minimum to helping automate the creation of a video.

# KdenliveFile.h vs. KdenliveProject.h
To create a .kdenlive file, you can either use the KdenliveFile class or KdenliveProject class.

### KdenliveFile: 
KdenliveFile is basically a wrapper for the XMLDocument class of the tinyxml library, so it offers the lowest level of control of the file.
However, this means that you can only add clips to a track sequentially, and must keep track of adding blanks to the track to ensure your clip starts at the correct time.
If you simply have a list of clips you want to play one after the other, and are sure you won't need to change their position after adding them, KdenliveFile is probably the best choice.

### KdenliveProject: 
KdenliveProject offers the functionality you would actually want when adding clips to the timeline. You can simply set the position of a clip, and don't need to worry about the order of entering them or which track they need to be on. When the file is generated, the clips will automatically be assigned tracks that will allow them to be placed at a specific position, and blanks are automatically inserted to ensure the clips starts at the correct position.

Additionally, KdenliveProject only needs to be given the names of your media files, and when you generate the .kdenlive file, you need to give it a list of paths to your resource folders.
The file will be searched for across those folders in order to find the exact filepath of the your media file. This allows for ease of automation, as you won't need to worry about generating exact filepaths for each and every media file.

Note that KdenliveProject uses KdenliveFile in it's implementation, so you would need to include KdenliveFile if you are to use KdenliveProject.

# Dependencies
The tinyxml2 library (https://github.com/leethomason/tinyxml2) is used to parse and edit the .kdenlive file, as the .kdenlive files use the XML format.
The tinyxml2 library has been included in the lib folder.
