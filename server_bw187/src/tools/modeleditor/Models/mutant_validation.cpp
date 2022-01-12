/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "mutant.hpp"

/**
 * This is a helper function which strip two strings down to their differing roots, e.g.
 *   strA = "sets/test/images"  strB = "sets/new_test/images"
 * becomes
 *   strA = "sets/test"         strB = "sets/new_test"

 *  @param	strA			The first string
 *	@param	strB			The second string
 **/
void Mutant::clipToDiffRoot( std::string& strA, std::string& strB )
{
	std::string::size_type posA = strA.find_last_of("/");
	std::string::size_type posB = strB.find_last_of("/");

	while ((posA != std::string::npos) &&
		(posB != std::string::npos) &&
		( strA.substr( posA ) == strB.substr( posB )))
	{
		strA = strA.substr( 0, posA );
		strB = strB.substr( 0, posB );

		posA = strA.find_last_of("/");
		posB = strB.find_last_of("/");
	}
}

/**
 * This method checks whether a texture could be an animated texture and fixes the animtex if needed.
 *
 *  @param	texRoot			The texture to test
 *	@param	oldRoot			The old root to replace
 *	@param	newRoot			The new root to replace with
 */
void Mutant::fixTexAnim( const std::string& texRoot, const std::string& oldRoot, const std::string& newRoot )
{
	std::string texAnimName = texRoot.substr( 0, texRoot.find_last_of(".") ) + ".texanim";

	DataSectionPtr texAnimFile =  BWResource::openSection( texAnimName );
	if (texAnimFile != NULL)
	{
		bool changed = false;

		std::vector< DataSectionPtr > textures;
		texAnimFile->openSections( "texture", textures );
		for (unsigned i=0; i<textures.size(); i++)
		{
			std::string textureName = textures[i]->asString("");
			if (textureName != "")
			{
				if (BWResource::fileExists( textureName )) continue;
				std::string::size_type pos = textureName.find( oldRoot );
				if (pos != std::string::npos)
				{
					textureName = textureName.replace( pos, oldRoot.length(), newRoot );
					textures[i]->setString( textureName );
					changed = true;
				}
			}
		}
		if (changed)
		{
			texAnimFile->save();
		}
	}
}

/**
 * This method fixes the texture paths of a <material> data section if needed.
 *
 *  @param	mat				The <material> data section to fix
 *	@param	oldRoot			The old root to replace
 *	@param	newRoot			The new root to replace with
 *
 *	@return Whether any texture paths were changed.
 */
bool Mutant::fixTextures( DataSectionPtr mat, const std::string& oldRoot, const std::string& newRoot )
{
	bool changed = false;
	
	std::string newMfm = mat->readString("mfm","");
	if (newMfm != "")
	{
		if (!BWResource::fileExists( newMfm ))
		{
			std::string::size_type pos = newMfm.find( oldRoot );
			if (pos != std::string::npos)
			{
				newMfm = newMfm.replace( pos, oldRoot.length(), newRoot );
				mat->writeString( "mfm", newMfm );
				changed = true;
			}
		}
	}
	
	instantiateMFM( mat );
						
	std::vector< DataSectionPtr > effects;
	mat->openSections( "fx", effects );
	for (unsigned i=0; i<effects.size(); i++)
	{
		std::string newFx = effects[i]->asString("");
		if (newFx != "")
		{
			if (BWResource::fileExists( newFx )) continue;
			std::string::size_type pos = newFx.find( oldRoot );
			if (pos != std::string::npos)
			{
				newFx = newFx.replace( pos, oldRoot.length(), newRoot );
				effects[i]->setString( newFx );
				changed = true;
			}
		}
	}

	std::vector< DataSectionPtr > props;
	mat->openSections( "property", props );
	for (unsigned i=0; i<props.size(); i++)
	{
		std::string newTexture = props[i]->readString( "Texture", "" );
		if (newTexture != "")
		{
			if (BWResource::fileExists( newTexture )) continue;
			std::string::size_type pos = newTexture.find( oldRoot );
			if (pos != std::string::npos)
			{
				newTexture = newTexture.replace( pos, oldRoot.length(), newRoot );
				props[i]->writeString( "Texture", newTexture );
				fixTexAnim( newTexture, oldRoot, newRoot );
				changed = true;
			}
		}
		else // Make sure to handle texture feeds as well
		{
			newTexture = props[i]->readString( "TextureFeed/default", "" );
			if (newTexture != "")
			{
				if (BWResource::fileExists( newTexture )) continue;
				std::string::size_type pos = newTexture.find( oldRoot );
				if (pos != std::string::npos)
				{
					newTexture = newTexture.replace( pos, oldRoot.length(), newRoot );
					props[i]->writeString( "TextureFeed/default", newTexture );
					fixTexAnim( newTexture, oldRoot, newRoot );
					changed = true;
				}
			}
		}
	}

	return changed;
}

/*static*/ std::vector< std::string > Mutant::s_missingFiles_;
/**
 * Static method to clear the list of files which couldn't be found
 */
/*static*/ void Mutant::clearFilesMissingList()
{
	s_missingFiles_.clear();
}

/**
 * This method tries to locate the file requested.
 *
 *  @param	fileName		Path of the missing file.
 *  @param	modelName		Path of the model trying to be opened.
 *  @param	ext				The extension of the file type to find (e.g. "model", "visual", etc)
 *  @param	what			What operation is being exectuted (e.g. "load", "add", etc)
 *  @param	criticalMsg		A string for whether the model can load without locating the missing file

 *	@param	oldRoot			The old root to replace
 *	@param	newRoot			The new root to replace with
 *
 *	@return Whether the file was located.
 */
bool Mutant::locateFile( std::string& fileName, std::string modelName, const std::string& ext, const std::string& what, const std::string& criticalMsg /* = "" */ )
{
	std::vector< std::string >::iterator entry = std::find( s_missingFiles_.begin(), s_missingFiles_.end(), fileName );

	if (entry != s_missingFiles_.end()) return false;
	
	s_missingFiles_.push_back( fileName );
		
	char msg[256];
	sprintf( msg, "Unable to locate %s file:\n\n  \"%s.%s\"\n\n"
		"Do you want to try and locate this file?%s",
		ext.c_str(), fileName.c_str(), ext.c_str(), criticalMsg.c_str() );
	char head[256];
	sprintf( head, "Unable to locate %s file", ext.c_str() );
	int response = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
		msg, head, MB_ICONERROR | MB_YESNO );
	
	if (response == IDYES)
	{
		std::string oldFileName = fileName.substr( fileName.find_last_of( "/" ) + 1 ) + "." + ext;

		char fileFilter[256];
		sprintf( fileFilter, "%s (*.%s)|*.%s||",
			ext.c_str(), ext.c_str(), ext.c_str() );
		CFileDialog fileDlg ( TRUE, "", oldFileName.c_str(), OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, fileFilter );

		modelName = BWResource::resolveFilename( modelName );
		std::replace( modelName.begin(), modelName.end(), '/', '\\' );
		fileDlg.m_ofn.lpstrInitialDir = modelName.c_str();

		if ( fileDlg.DoModal() == IDOK )
		{
			fileName = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));
			fileName = fileName.substr( 0, fileName.rfind(".") );
			return true;
		}
	}
	
	return false;
}

/**
 * This method tests the validity of the model requested.
 *
 *  @param	name			Path of the model to test for validity.
 *  @param	what			What operation is being exectuted (e.g. "load", "add", etc)

 *  @param	model			The datasection of the model to return
 *  @param	visual			The datasection of the visual to return
 *	@param	visualName		The path of the visual file to return
 *	@param	primitivesName	The path of the primitives file to return
 *
 *	@return Whether the file was valid.
 */
bool Mutant::ensureModelValid( const std::string& name, const std::string& what,
	DataSectionPtr* model /* = NULL */, DataSectionPtr* visual /* = NULL */,
	std::string* visualName /* = NULL */, std::string* primitivesName /* = NULL */  )
{
	//This will get the data section for the visual file,
	// this is used for the node heirarchy
	DataSectionPtr testModel = BWResource::openSection( name, false );
	DataSectionPtr testVisual = NULL;
	std::string testRoot = "";

	if (testModel)
	{
		testRoot = testModel->readString("nodefullVisual","");
		std::string oldRoot;
		std::string newRoot;

		if (testRoot != "")
		{
			testVisual = BWResource::openSection( testRoot + ".visual", false );

			if (!testVisual)
			{
				oldRoot = testRoot;

				if (!locateFile( testRoot, name, "visual", what,
					"\n\nYou will be unable to load this model otherwise." ))
					return false;

				newRoot = testRoot;
				testVisual = BWResource::openSection( testRoot + ".visual", false );
				testModel->writeString( "nodefullVisual", testRoot );

				newRoot = newRoot.substr( 0, newRoot.find_last_of( "/" ));
				oldRoot = oldRoot.substr( 0, oldRoot.find_last_of( "/" ));
				clipToDiffRoot( newRoot, oldRoot );

				//Fix all the animation references
				std::vector< DataSectionPtr > anims;
				testModel->openSections( "animation", anims );

				for (unsigned i=0; i<anims.size(); i++)
				{
					std::string animRoot = anims[i]->readString( "nodes", "" );
					if (BWResource::fileExists( animRoot + ".animation" )) continue;
					std::string::size_type pos = animRoot.find( oldRoot );
					if (pos != std::string::npos)
					{
						animRoot = animRoot.replace( pos, oldRoot.length(), newRoot );
						anims[i]->writeString( "nodes", animRoot );
					}
				}

				//Fix all the tint references
				std::vector< DataSectionPtr > dyes;
				testModel->openSections( "dye", dyes );
				for (unsigned i=0; i<dyes.size(); i++)
				{
					std::vector< DataSectionPtr > tints;
					dyes[i]->openSections( "tint", tints );
					for (unsigned j=0; j<tints.size(); j++)
					{
						fixTextures( tints[j]->openSection("material"), oldRoot, newRoot );
					}
				}

				testModel->save();
			}
		}
		else
		{
			testRoot = testModel->readString("nodelessVisual","");

			if (testRoot != "")
			{
				testVisual = BWResource::openSection( testRoot + ".visual", false );

				if (!testVisual)
				{
					oldRoot = testRoot;

					if (!locateFile( testRoot, name, "visual", what,
						"\n\nYou will be unable to load this model otherwise." ))
						return false;
					
					newRoot = testRoot;
					testVisual = BWResource::openSection( testRoot + ".visual", false );
					testModel->writeString( "nodelessVisual", testRoot );
					
					newRoot = newRoot.substr( 0, newRoot.find_last_of( "/" ));
					oldRoot = oldRoot.substr( 0, oldRoot.find_last_of( "/" ));
					clipToDiffRoot( newRoot, oldRoot );

					testModel->save();
				}
			}
			else
			{
				testRoot = testModel->readString("billboardVisual","");

				if (testRoot != "")
				{
					testVisual = BWResource::openSection( testRoot + ".visual", false );

					if (!testVisual)
					{
						oldRoot = testRoot;

						if (!locateFile( testRoot, name, "visual", what,
							"\n\nYou will be unable to load this model otherwise." ))
							return false;

						newRoot = testRoot;
						testVisual = BWResource::openSection( testRoot + ".visual", false );
						testModel->writeString( "billboardVisual", testRoot );

						newRoot = newRoot.substr( 0, newRoot.find_last_of( "/" ));
						oldRoot = oldRoot.substr( 0, oldRoot.find_last_of( "/" ));
						clipToDiffRoot( newRoot, oldRoot );

						testModel->save();
					}
				}
				else
				{
					ERROR_MSG( "Cannot determine the visual type of the model\"%s\".\n"
						"Unable to %s model.", name.c_str(), what.c_str() );
					return false;
				}
			}
		}

		// Update any references in the visual file if needed
		if (newRoot != oldRoot)
		{
			bool visualChange = false;

			std::vector< DataSectionPtr > renderSets;
			testVisual->openSections( "renderSet", renderSets );
			for (unsigned i=0; i<renderSets.size(); i++)
			{
				std::vector< DataSectionPtr > geometries;
				renderSets[i]->openSections( "geometry", geometries );
				for (unsigned j=0; j<geometries.size(); j++)
				{
					std::string::size_type pos;
					std::string newVerts = geometries[j]->readString("vertices","");
					pos = newVerts.find( oldRoot );
					if (pos != std::string::npos)
					{
						newVerts = newVerts.replace( pos, oldRoot.length(), newRoot );
						geometries[j]->writeString( "vertices", newVerts );
						visualChange = true;
					}

					std::string newPrims = geometries[j]->readString("primitive","");
					pos = newPrims.find( oldRoot );
					if (pos != std::string::npos)
					{
						newPrims = newPrims.replace( pos, oldRoot.length(), newRoot );
						geometries[j]->writeString( "primitive", newPrims );
						visualChange = true;
					}

					std::vector< DataSectionPtr > primGroups;
					geometries[j]->openSections( "primitiveGroup", primGroups );
					for (unsigned k=0; k<primGroups.size(); k++)
					{
						std::vector< DataSectionPtr > materials;
						primGroups[k]->openSections( "material", materials );
						for (unsigned l=0; l<materials.size(); l++)
						{
							visualChange = fixTextures( materials[l], oldRoot, newRoot ) || visualChange;
						}
					}
				}
			}
		
			if (visualChange)
			{
				testVisual->save();
			}
		}

		if ( ! BWResource::openSection( testRoot + ".primitives" ) )
        {
			ERROR_MSG( "Unable to find the primitives file for \"%s\".\n\n"
				"Make sure that \"%s.primitives\" exists and is in the same directory as the *.visual file.\n\n"
				"Unable to %s model.\n", name.c_str(), testRoot.c_str(), what.c_str() );

			return false;
        }

		// Make sure the parent model exists and can be found
		std::string oldParentRoot = testModel->readString( "parent", "" );
		std::string parentRoot = oldParentRoot;
		if (oldParentRoot != "")
		{
			DataSectionPtr testParentModel = BWResource::openSection( oldParentRoot + ".model", false );
			if (testParentModel == NULL)
			{
				parentRoot = oldParentRoot;
				if (locateFile( parentRoot, name, "model", what ))
				{
					testModel->writeString( "parent", parentRoot );
					testModel->save();
				}
			}

			if (!ensureModelValid( parentRoot + ".model", what )) return false;
		}
	}
	else
	{
		// this means that no model file could be opened
		return false;
	}

	// Update any parameters requested
	
	if (model)
		*model = testModel;

	if (visual)
		*visual = testVisual;

	if (visualName)
		*visualName = testRoot + ".visual";

	if (primitivesName)
		*primitivesName = testRoot + ".primitives";
	
	return true;
}
