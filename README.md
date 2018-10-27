# Slipgate

## About
Slipgate is a Quake engine source port based on Quakespasm 0.93.0. The primary goal of this port is to provide an engine that is more suitable for developing full standalone games while still keeping compatibility with stock quake and quake mods. Some basic examples are being able to load normally awkward asset types such as lmp files directly as as tga files, modernizing some engine code, and a Lua client/server scripting system.

## Primary Features and Changes
 - A Lua based client and server scripting interface that interops with QuakeC. See the **QLua** section for more info.
 
 - Modified player physics allowing high frame rate. Quakes standard physics generally break down and cause sticking and other issues above 70 fps.
 
 - Added cvars to use affine uv coordinate interpolation to alias models and vertex position rounding to both alias models and brush models.
 
 - A work in progress persistant blood splat tech based on the existing lightmapping implementation. Splats are added with a special kind of light that is marked as a splat, and renders into the splat map instead of the light map. The existing QLua code uses it to create blood splats with a red light color. Ideally this would be inhanced with more interesting features in the shader such as splat pattern masking based on blood splat intensity.
 
 - A simple particle system that all the built in quake particles now use. It can be used from QLua or C to create what custom particle effects.
 
 - Some modernization of graphics code, such as moving the BSP rendering from fixed function to shaders. Removal of some legacy code paths and SDL1 support.
 
 - Changing vec3_t and related types to from typedef float[3] to dedicated structs, and massive refactor of all of vector math usage. This results in much cleaner vector math and avoids the need for temporary variables. For instance you can write something like `vec3_t result = Vec_Cross(Vec_Sub(endPos, startPos), normal)`. Some additional math and vector functions were also added.
 
 - Server entities now have unique IDs that an be used to identify entities. The ID is also replicated to the client and available in QLua.
 
 - Fixed an issue with floating point mouse movement data being converted to integers, causing mouse motion loss which was very noticable at higher frame reates.

## QLua
QLua is a Lua based scripting interface for the engine consisting of 2 separate lua states.
#### Server State
The server code and can call into, get return values from, override functions, and access global variables of QuakeC. It can do everything you can do in QuakeC and much more. The intent is to use use QuakeC as the game base and then add new features and override existing features in Lua which is a much more complete and versatile language.
#### Client State
The client code consumes client entities (the data that quake sends to clients for rendering). These entities are much simpler than the server side versions, although a new server protocol in introduced which gives some additional useful entity information to the client, including replicated lua variables from the server.
#### RPC Calls
The server can invoke arbitrary lua code on the client by using RPC calls. This can be useful for invoking events on the client. For example you could tell the client to spawn a gib at a specific location with a specific velocity and let the client handle all of its rendering, physics, and particle logic, instead of creating and replicating a gib as a server entity.

id1 contains the QLua runtime folder and some test stuff like more gore and blood splats. If you want vanilla quake, just remove or rename the id1/qlua directory.

## Downloading
The /slipgate/ folder contains windows 64 bit binaries and an id1 folder structure. If you don't care about the source, you can download the /slipgate/ and put `PAK0.PAK` and `PAK1.PAK` in the id1 folder and play. There is a `config.cfg` in there already with some sane modern graphics and control settings.

## Compiling
Only the 64 bit windows build has been maintained. The mac and linux build systems will need to be updated with new source files and luajit library needs to be provided. I don't think there is any cross platform code breakage but it is untested.
