/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

namespace ChunkFormat
{

inline std::string outsideChunkIdentifier( int gridX, int gridZ, bool singleDir = false )
{
	char chunkIdentifierCStr[32];
	std::string gridChunkIdentifier;

    uint16 gridxs = uint16(gridX), gridzs = uint16(gridZ);
	if (!singleDir)
	{
		if (uint16(gridxs + 4096) >= 8192 || uint16(gridzs + 4096) >= 8192)
		{
				sprintf( chunkIdentifierCStr, "%01xxxx%01xxxx/sep/",
						int(gridxs >> 12), int(gridzs >> 12) );
				gridChunkIdentifier = chunkIdentifierCStr;
		}
		if (uint16(gridxs + 256) >= 512 || uint16(gridzs + 256) >= 512)
		{
				sprintf( chunkIdentifierCStr, "%02xxx%02xxx/sep/",
						int(gridxs >> 8), int(gridzs >> 8) );
				gridChunkIdentifier += chunkIdentifierCStr;
		}
		if (uint16(gridxs + 16) >= 32 || uint16(gridzs + 16) >= 32)
		{
				sprintf( chunkIdentifierCStr, "%03xx%03xx/sep/",
						int(gridxs >> 4), int(gridzs >> 4) );
				gridChunkIdentifier += chunkIdentifierCStr;
		}
	}
	sprintf( chunkIdentifierCStr, "%04x%04xo", int(gridxs), int(gridzs) );
	gridChunkIdentifier += chunkIdentifierCStr;

	return gridChunkIdentifier;
}

};
