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
	unsigned int x{ 0 }, y{ 0 }, z{ 0 }, w{ 0 };
};

static struct Environment {
	const char* filePath{ nullptr };
	const char* outPath{ "dump.obj" };
	unsigned long startOffset{ 0 };
	unsigned long stride{ 0 };
	unsigned long endOffset{ 0 };

	float scale{ 1.0f };
    enum class VertexType {
        F32,
        I16,
    } vertexType{ VertexType::F32 };

	unsigned long faceStartOffset{ 0 };
	unsigned long faceEndOffset{ 0 };
	unsigned long faceStride{ 0 };
    enum class FaceType {
        I16,
        I32,
    } faceType{ FaceType::I32 };
    bool faceQuad{ false };
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
static void SetVertexType( const char* argument) { env.vertexType = (Environment::VertexType)strtoul(argument, nullptr, 10); }
static void SetFaceStartOffset(const char* argument) { env.faceStartOffset = strtoul(argument, nullptr, 10); }
static void SetFaceEndOffset(const char* argument) { env.faceEndOffset = strtoul(argument, nullptr, 10); }
static void SetFaceStride(const char* argument) { env.faceStride = strtoul(argument, nullptr, 10); }
static void SetFaceType(const char* argument) { env.faceType = (Environment::FaceType)strtoul(argument, nullptr, 10); }
static void SetFaceQuad(const char* argument) { env.faceQuad = true; }
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
        { "-vtyp", SetVertexType, "Sets how the vertex bytes are stored.\n"
                                  "0 = float32 (default), 1 = int16" },
		{ "-fsof", SetFaceStartOffset, "Sets the start offset to start loading face indices from." },
		{ "-feof", SetFaceEndOffset, "Sets the end offset to finish loading face indices from." },
		{ "-fstr", SetFaceStride, "Number of bytes to proceed after reading in face indices." },
        { "-ftyp", SetFaceType, "Sets how the face bytes are stored.\n"
                                "0 = int16, 1 = int32" },
        { "-fquad", SetFaceQuad, "Indicates that the faces are made up of four elements, a quad." },
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
        bool success = false;
        switch( env.vertexType ) {
            default: {
                if (fread(&v, sizeof(Vertex), 1, file) != 1) {
                    break;
                }
                success = true;
                break;
            }
            case Environment::VertexType::I16: {
                int16_t coords[3];
                if (fread(coords, sizeof(int16_t), 3, file) != 3) {
                    break;
                }
                // blergh...
                v.x = (float) coords[0];
                v.z = (float) coords[2];
                v.y = (float) coords[1];
                success = true;
                break;
            }
        }

        if ( !success ) {
            Print("Failed to read in vertex at %lu\n", ftell(file));
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
		int r = fseek(file, ( long ) env.stride, SEEK_CUR);
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

        unsigned int varSize;
        switch( env.faceType ) {
            default:
                varSize = sizeof( uint32_t );
                break;
            case Environment::FaceType::I16:
                varSize = sizeof( uint16_t );
                break;
        }

		// Since we require both the start and end, we know how much data we want.
        unsigned int numFaces = faceBytes / (varSize * (env.faceQuad ? 4 : 3));
		env.meshFaces.reserve( numFaces );
		for( unsigned int i = 0; i < numFaces; ++i ) {
            // Quick crap to deal with stride
            long offset = ftell( file );
            if ( offset > env.faceEndOffset )
                break;

			Face f;
            switch ( env.faceType ) {
                default: {
                    if ( env.faceQuad ) {
                        if (fread(&f, sizeof(Face), 1, file) != 1 ) {
                            Warn( "Failed to read in face (%u), some faces may be missing or incorrect!\n", i );
                        }
                    } else {
                        uint32_t x, y, z;
                        if (fread(&x, sizeof(uint32_t), 1, file) != 1) {
                            Warn("Failed to load in face element x (%u), some faces may be missing or incorrect!\n",
                                 i);
                            break;
                        }
                        f.x = x;
                        if (fread(&y, sizeof(uint32_t), 1, file) != 1) {
                            Warn("Failed to load in face element y (%u), some faces may be missing or incorrect!\n",
                                 i);
                            break;
                        }
                        f.y = y;
                        if (fread(&z, sizeof(uint32_t), 1, file) != 1) {
                            Warn("Failed to load in face element z (%u), some faces may be missing or incorrect!\n",
                                 i);
                            break;
                        }
                        f.z = z;
                    }
                    break;
                }
                case Environment::FaceType::I16: {
                    uint16_t x, y, z;
                    if (fread(&x, sizeof(uint16_t), 1, file) != 1) {
                        Warn("Failed to load in face element x (%u), some faces may be missing or incorrect!\n", i );
                        break;
                    }
                    f.x = x;
                    if (fread(&y, sizeof(uint16_t), 1, file) != 1) {
                        Warn("Failed to load in face element y (%u), some faces may be missing or incorrect!\n", i );
                        break;
                    }
                    f.y = y;
                    if (fread(&z, sizeof(uint16_t), 1, file) != 1) {
                        Warn("Failed to load in face element z (%u), some faces may be missing or incorrect!\n", i );
                        break;
                    }
                    f.z = z;
                    if ( env.faceQuad ) {
                        uint16_t w;
                        if ( fread( &w, sizeof( uint16_t ), 1, file ) != 1 ) {
                            Warn("Failed to load in face element x (%u), some faces may be missing or incorrect!\n", i );
                            break;
                        }
                        f.w = w;
                    }
                    break;
                }
            }

            if ( env.faceQuad ) {
                VPrint("\tx( %u ) y( %u ) z( %u ) w( %u )\n", f.x, f.y, f.z, f.w);
                if (f.x >= env.meshVertices.size() || f.y >= env.meshVertices.size() ||
                    f.z >= env.meshVertices.size() || f.w >= env.meshVertices.size() ) {
                    Warn("Encountered out of bound vertex index, ");
                    if (f.x >= env.meshVertices.size()) {
                        Print("X (%u)", f.x);
                        f.x = 0;
                    }
                    if (f.y >= env.meshVertices.size()) {
                        Print("Y (%u)", f.y);
                        f.y = 0;
                    }
                    if (f.z >= env.meshVertices.size()) {
                        Print("Z (%u)", f.z);
                        f.z = 0;
                    }
                    if (f.z >= env.meshVertices.size()) {
                        Print("W (%u)", f.w);
                        f.w = 0;
                    }
                    Print("- defaulting to 0!\n");
                }
            } else {
                VPrint("\tx( %d ) y( %d ) z( %d )\n", f.x, f.y, f.z);
                if (f.x >= env.meshVertices.size() || f.y >= env.meshVertices.size() ||
                    f.z >= env.meshVertices.size()) {
                    Warn("Encountered out of bound vertex index, ");
                    if (f.x >= env.meshVertices.size()) {
                        Print("X (%u)", f.x);
                        f.x = 0;
                    }
                    if (f.y >= env.meshVertices.size()) {
                        Print("Y (%u)", f.y);
                        f.y = 0;
                    }
                    if (f.z >= env.meshVertices.size()) {
                        Print("Z (%u)", f.z);
                        f.z = 0;
                    }
                    Print("- defaulting to 0!\n");
                }
            }
			env.meshFaces.push_back(f);
			int r = fseek( file, ( long ) env.faceStride, SEEK_CUR );
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

    unsigned int numFaceElements = env.faceQuad ? 4 : 3;
	for( auto &face : env.meshFaces ) {
        auto *fv = (unsigned int *) &face;
        bool invalid = false;
        for (unsigned int i = 0; i < numFaceElements; ++i) {
            for (unsigned int j = 0; j < numFaceElements; ++j) {
                if (fv[i] == fv[j] && i != j) {
                    VPrint("Invalid face indices found (%u %u %u)!\n", fv[0], fv[1], fv[2]);
                    invalid = true;
                    break;
                }
                if (invalid) break;
            }
        }

        if (invalid)
            continue;

        fprintf(file, "f ");
        for (unsigned int i = 0; i < numFaceElements; ++i) {
            fprintf(file, i != (numFaceElements - 1) ? "%d " : "%d\n", fv[i] + 1);
        }
	}
	CloseFile(file);

	Print("Wrote \"%s\"!\n", env.outPath);

	return EXIT_SUCCESS;
}
