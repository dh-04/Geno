/*
 * Copyright (c) 2021 Sebastian Kylander https://gaztin.com/
 *
 * This software is provided 'as-is', without any express or implied warranty. In no event will
 * the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the
 *    original software. If you use this software in a product, an acknowledgment in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as
 *    being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "CompilerMSVC.h"

#include <Common/Process.h>

#include <Windows.h>

#if defined( _WIN64 )
#define HOST "Hostx64"
#else // _WIN64
#define HOST "Hostx86"
#endif // _WIN64

//////////////////////////////////////////////////////////////////////////

static std::filesystem::path FindProgramFilesX86Dir( void )
{
	if( DWORD ProgramFilesLength = GetEnvironmentVariableW( L"ProgramFiles(x86)", nullptr, 0 ) )
	{
		std::wstring ProgramFilesBuffer;
		ProgramFilesBuffer.resize( ProgramFilesLength );

		GetEnvironmentVariableW( L"ProgramFiles(x86)", ProgramFilesBuffer.data(), ProgramFilesLength );

		// Remove extra null-terminator
		ProgramFilesBuffer.pop_back();

		return ProgramFilesBuffer;
	}

	return std::filesystem::path();

} // FindProgramFilesX86Dir

//////////////////////////////////////////////////////////////////////////

static std::filesystem::path FindMSVCDir( const std::filesystem::path& rProgramFilesX86 )
{
	const std::filesystem::path VSWhereLocation = rProgramFilesX86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
	if( std::filesystem::exists( VSWhereLocation ) )
	{
		// Run vswhere.exe to get the installation path of Visual Studio
		int                Result;
		const std::wstring VSWhereOutput = Process::OutputOf( VSWhereLocation.wstring() + L" -latest -property installationPath", Result );
		if( Result == 0 )
		{
			// Trim trailing newlines
			const std::filesystem::path VisualStudioLocation( VSWhereOutput.begin(), VSWhereOutput.end() - 2 );
			if( std::filesystem::exists( VisualStudioLocation ) )
			{
				const std::filesystem::path CL = VisualStudioLocation / "VC" / "Tools" / "MSVC" / "14.28.29910";
				if( std::filesystem::exists( CL ) )
					return CL;
			}
		}
	}

	return std::filesystem::path();

} // FindMSVCDir

//////////////////////////////////////////////////////////////////////////

static std::wstring FindWindowsSDKVersion( const std::filesystem::path& rProgramFilesX86 )
{
	for( const std::filesystem::directory_entry& rDirectory : std::filesystem::directory_iterator( rProgramFilesX86 / "Windows Kits" / "10" / "Lib" ) )
	{
		if( std::filesystem::exists( rDirectory / "um" / "x64" / "kernel32.lib" ) )
			return rDirectory.path().filename();
	}

	return std::wstring();

} // FindWindowsSDKVersion

//////////////////////////////////////////////////////////////////////////

std::wstring CompilerMSVC::MakeCommandLineString( const CompileOptions& rOptions )
{
	const std::filesystem::path ProgramFilesX86 = FindProgramFilesX86Dir();
	const std::filesystem::path MSVCDir         = FindMSVCDir( ProgramFilesX86 );

	std::wstring CommandLine;
	CommandLine += ( MSVCDir / "bin" / HOST / "x64" / "cl.exe" ).wstring();
	CommandLine += L" /c /nologo /EHsc";

	// Set standard include directories
	{
		const std::wstring          WindowsSDKVersion    = FindWindowsSDKVersion( ProgramFilesX86 );
		const std::filesystem::path WindowsSDKIncludeDir = ProgramFilesX86 / "Windows Kits" / "10" / "Include" / WindowsSDKVersion;

		CommandLine += L" /I\"" + ( MSVCDir / "include"           ).wstring() + L"\"";
		CommandLine += L" /I\"" + ( WindowsSDKIncludeDir / "ucrt" ).wstring() + L"\"";
	}

	// Set output file
	CommandLine += L" " + rOptions.InputFile.wstring();

	return CommandLine;

} // MakeCommandLineString

//////////////////////////////////////////////////////////////////////////

std::wstring CompilerMSVC::MakeCommandLineString( const LinkOptions& rOptions )
{
	const std::filesystem::path ProgramFilesX86   = FindProgramFilesX86Dir();
	const std::filesystem::path MSVCDir           = FindMSVCDir( ProgramFilesX86 );
	const std::wstring          WindowsSDKVersion = FindWindowsSDKVersion( ProgramFilesX86 );
	std::wstring                CommandLine;

	CommandLine += ( MSVCDir / "bin" / HOST / "x64" / "link.exe" ).wstring();
	CommandLine += L" /NOLOGO /MACHINE:x64";

	// Add standard library paths
	{
		const std::filesystem::path WindowsSDKLibraryDir = ProgramFilesX86 / "Windows Kits" / "10" / "Lib" / WindowsSDKVersion;

		CommandLine += L" /LIBPATH:\"" + ( MSVCDir / "lib" / "x64"               ).wstring() + L"\"";
		CommandLine += L" /LIBPATH:\"" + ( WindowsSDKLibraryDir / "um" / "x64"   ).wstring() + L"\"";
		CommandLine += L" /LIBPATH:\"" + ( WindowsSDKLibraryDir / "ucrt" / "x64" ).wstring() + L"\"";
	}

	switch( rOptions.Kind )
	{
		case Project::Kind::Application:
		{
			CommandLine += L" /SUBSYSTEM:CONSOLE";

		} break;
	}

	for( const std::filesystem::path& rInputFile : rOptions.InputFiles )
	{
		CommandLine += L" " + rInputFile.wstring();
	}

	CommandLine += L" /OUT:" + rOptions.OutputFile.wstring();

	return CommandLine;

} // MakeCommandLineString
