/*
MIT License

Copyright (c) 2021 Mark E Sowden <hogsy@oldtimes-software.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstring>
#include <cmath>

#include <iostream>
#include <vector>

struct Vertex {
	float x{ 0 }, y{ 0 }, z{ 0 };

	void operator*=( float v ) { x *= v; y *= v; z *= v; }
};

struct Face {
	unsigned int x{ 0 }, y{ 0 }, z{ 0 };
};

static struct Environment {
	const char* filePath{ nullptr };
	const char* outPath{ "dump.obj" };
	unsigned long startOffset{ 0 };
	unsigned long stride{ 0 };
	unsigned long endOffset{ 0 };
	
	float scale{ 1.0f };

	unsigned long faceStartOffset{ 0 };
	unsigned long faceEndOffset{ 0 };
	unsigned long faceStride{ 0 };
	std::vector<Face> meshFaces;

	bool verbose{ false };

	std::vector<Vertex> meshVertices;
} env;

#define AbortApp( ... ) printf( __VA_ARGS__ ); exit( EXIT_FAILURE )
#define Print( ... )	printf( __VA_ARGS__ )
#define VPrint( ... )	if( env.verbose ) { printf( __VA_ARGS__ ); }
#define Warn( ... )		printf( "WARNING: " __VA_ARGS__ )

static void SetOutPath(const char* argument) { env.outPath = argument; }
static void SetStartOffset(const char* argument) { env.startOffset = strtoul(argument, nullptr, 10); }
static void SetEndOffset(const char* argument) { env.endOffset = strtoul(argument, nullptr, 10); }
static void SetStride(const char* argument) { env.stride = strtoul(argument, nullptr, 10); }
static void SetVertexScale(const char* argument) { env.scale = strtof(argument, nullptr); }
static void SetFaceStartOffset(const char* argument) { env.faceStartOffset = strtoul(argument, nullptr, 10); }
static void SetFaceEndOffset(const char* argument) { env.faceEndOffset = strtoul(argument, nullptr, 10); }
static void SetFaceStride(const char* argument) { env.faceStride = strtoul(argument, nullptr, 10); }
static void SetVerboseMode(const char* argument) { env.verbose = true; }

/**
 * Parse all arguments on the command line based on the provided table.
 */
static void ParseCommandLine(int argc, char** argv) {
	struct LaunchArgument {
		const char* str;
		void(*Callback)(const char* argument);
		const char* desc;
	};
	// All possible arguments go in this table.
	static LaunchArgument launchArguments[] = {
		{ "-soff", SetStartOffset, "Set the start offset to begin reading from." },
		{ "-eoff", SetEndOffset, "Set the end offset to stop reading, otherwise reads to EOF." },
		{ "-stri", SetStride, "Number of bytes to proceed after reading XYZ." },
		{ "-outp", SetOutPath, "Set the path for the output file." },
		{ "-vtxs", SetVertexScale, "Scales the vertices by the defined amount." },
		{ "-fsof", SetFaceStartOffset, "Sets the start offset to start loading face indices from." },
		{ "-feof", SetFaceEndOffset, "Sets the end offset to finish loading face indices from." },
		{ "-fstr", SetFaceStride, "Number of bytes to proceed after reading in face indices." },
		{ "-verb", SetVerboseMode, "Enables more verbose output." },
		{ nullptr }
	};

	// If we don't have any arguments, print them out.
	if (argc <= 1) {
		Print("No arguments provided. Possible arguments are provided below.\n");
		Print("First argument is required to be a path to the file, then followed by any of the optional arguments.\n");
		const LaunchArgument* opt = &launchArguments[0];
		while (opt->str != nullptr) {
			Print("   %s\t\t%s\n", opt->str, opt->desc);
			opt++;
		}
		Print("For example,\n\tbin2obj ..\\path\\myfile.whatever -soff 128\n");
		exit(EXIT_SUCCESS);
		return;
	}

	const LaunchArgument* opt = &launchArguments[0];
	while (opt->str != nullptr) {
		for (int i = 0; i < argc; ++i) {
			if (strcmp(opt->str, argv[i]) != 0) {
				continue;
			}

			const char* arg = (i + 1) < argc ? argv[i + 1] : nullptr;
			opt->Callback(arg);
		}
		opt++;
	}
}

static void FileSeek(FILE* file, unsigned long numBytes, bool fromStart) {
	if (fseek(file, numBytes, fromStart ? SEEK_SET : SEEK_CUR) == 0) {
		return;
	}

	AbortApp("Failed to seek to %lu!\n", numBytes);
}

#define CloseFile(FILE) if( (FILE) != nullptr ) fclose( (FILE) ); (FILE) = nullptr

int main(int argc, char** argv) {
	Print(
		"Bin2Obj by Mark \"hogsy\" Sowden <hogsy@oldtimes-software.com>\n"
		"==============================================================\n\n"
	);

	ParseCommandLine(argc, argv);

	env.filePath = argv[1];
	Print("Loading \"%s\"\n", env.filePath);

	FILE* file = fopen(env.filePath, "rb");
	if (file == nullptr) {
		AbortApp("Failed to open \"%s\"!\n", env.filePath);
	}

	FileSeek(file, env.startOffset, true);

	while (feof(file) == 0) {
		Vertex v;
		if (fread(&v, sizeof(Vertex), 1, file) != 1) {
			break;
		}
		v *= env.scale;
		if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z)) {
			Warn("Encountered NaN for vertex, ");
			if (std::isnan(v.x)) { Print("X "); v.x = 0.0f; }
			if (std::isnan(v.y)) { Print("Y "); v.y = 0.0f; }
			if (std::isnan(v.z)) { Print("Z "); v.z = 0.0f; }
			Print("- defaulting to 0.0!\n");
		}
		VPrint( "\tx( %f ) y( %f ) z( %f )\n", v.x, v.y, v.z );
		env.meshVertices.push_back(v);
		if (env.endOffset > 0 && ftell(file) >= env.endOffset) {
			break;
		}
		int r = fseek(file, env.stride, SEEK_CUR);
		if (env.stride > 0 && r != 0) {
			break;
		}
	}
	Print( "Loaded in %d vertices\n", (int)env.meshVertices.size() );
	// If both start and end offsets are defined for the faces, load those in.
	unsigned long faceBytes = env.faceEndOffset - env.faceStartOffset;
	if( faceBytes > 0 ) {
		Print("Attempting to read in faces...\n");
		FileSeek( file, env.faceStartOffset, true );
		// Since we require both the start and end, we know how much data we want.
		unsigned int numFaces = faceBytes / sizeof( Face );
		env.meshFaces.reserve( numFaces );
		for( unsigned int i = 0; i < numFaces; ++i ) {
			Face f;
			if( fread( &f, sizeof( Face ), 1, file ) != 1 ) {
				Warn( "Failed to load in all desired faces, keep in mind some faces may be missing or incorrect!\n" );
				break;
			}
			VPrint( "\tx( %d ) y( %d ) z( %d )\n", f.x, f.y, f.z );
			if( f.x >= env.meshVertices.size() || f.y >= env.meshVertices.size() || f.z >= env.meshVertices.size() ) {
				Warn( "Encountered out of bound vertex index, " );
				if( f.x >= env.meshVertices.size() ) { Print( "X " ); f.x = 0; }
				if( f.y >= env.meshVertices.size() ) { Print( "Y " ); f.y = 0; }
				if( f.z >= env.meshVertices.size() ) { Print( "Z " ); f.z = 0; }
				Print( "- defaulting to 0!\n" );
			}
			env.meshFaces.push_back(f);
			int r = fseek( file, env.faceStride, SEEK_CUR );
			if( env.faceStride > 0 && r != 0 ) {
				break;
			}
		}
		Print( "Loaded in %d faces\n", (int)env.meshFaces.size() );
	}
	CloseFile(file);

	file = fopen(env.outPath, "w");
	fprintf(file, "# Generated by Bin2Obj, by Mark \"hogsy\" Sowden <hogsy@oldtimes-software.com>\n\n");
	for (auto& vertex : env.meshVertices) {
		fprintf(file, "v %f %f %f\n", vertex.x, vertex.y, vertex.z);
	}
	for( auto &face : env.meshFaces ) {
		fprintf(file, "f %d %d %d\n", face.x, face.y, face.z);
	}
	CloseFile(file);

	Print("Wrote \"%s\"!\n", env.outPath);

	return EXIT_SUCCESS;
}
