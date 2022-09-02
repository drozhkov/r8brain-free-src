/**
 * @file example.cpp
 *
 * @brief An example C++ file that demonstrates resampler's usage.
 *
 * This is an example file which you won't be able to compile as it includes
 * undisclosed WAVE audio file input-output code. Please consider this example
 * as a pseudo-code demonstrating the use of the library. Here you can find an
 * example implementation of the simplest sample rate converter utility.
 *
 * r8brain-free-src Copyright (c) 2013-2021 Aleksey Vaneev
 * See the "LICENSE" file for license.
 */

#include <string>
#include <memory>
#include <iostream>
#include <chrono>

#include "r8brain-free-src/CDSPResampler.h"

#include "CWaveFile.hpp"


using namespace r8b;


int main( int argc, char *argv[] )
{
	if ( argc < 3 )
	{
		std::cout << argv[ 0 ] << ": <in-wav> <out-wav>" << std::endl;
		return -1;
	}

	const double OutSampleRate = 96000.0;

	CWaveFile inf;
	// inf.loadFile( "Audio.wav" );
	inf.loadFile( argv[ 1 ] );

	std::cout << inf.ChannelCount << ", " << inf.SampleRate << ", "
			  << inf.BitsPerSample << std::endl;

	CWaveFile outf;
	outf.inheritFormat( inf );
	outf.SampleRate = OutSampleRate;
	// outf.saveFile( "AudioOut.wav" );
	outf.saveFile( argv[ 2 ] );

	const int InBufCapacity = 1024;
	auto InBufs = std::shared_ptr< CFixedBuffer< double >[] >(
		new CFixedBuffer< double >[ inf.ChannelCount ] );

	auto Resamps = std::shared_ptr< CPtrKeeper< CDSPResampler24 * >[] >(
		new CPtrKeeper< CDSPResampler24 * >[ inf.ChannelCount ] );

	for ( int i = 0; i < inf.ChannelCount; i++ )
	{
		InBufs[ i ].alloc( InBufCapacity );
		Resamps[ i ] =
			new CDSPResampler24( inf.SampleRate, OutSampleRate, InBufCapacity );
	}

	long long ol = inf.SampleCount * OutSampleRate / inf.SampleRate;
	std::chrono::duration< double > processingTime( 0 );

	while ( ol > 0 )
	{
		int ReadCount;
		inf.readData( InBufs.get(), InBufCapacity, ReadCount );

		if ( ReadCount == -1 )
		{
			ReadCount = InBufCapacity;

			for ( int i = 0; i < inf.ChannelCount; i++ )
			{
				memset( &InBufs[ i ][ 0 ], 0, ReadCount * sizeof( double ) );
			}
		}

		auto opp =
			std::shared_ptr< double *[] >( new double *[ inf.ChannelCount ] );

		int WriteCount; // At initial steps this variable can be equal to 0
						// after resampler. Same number for all channels.

		for ( int i = 0; i < inf.ChannelCount; i++ )
		{
			auto startTs = std::chrono::steady_clock::now();

			WriteCount =
				Resamps[ i ]->process( InBufs[ i ], ReadCount, opp[ i ] );

			processingTime += ( std::chrono::steady_clock::now() - startTs );
		}

		// std::cout << WriteCount << std::endl;

		if ( WriteCount > ol )
		{
			WriteCount = (int)ol;
		}

		outf.writeData( opp.get(), WriteCount );
		ol -= WriteCount;
	}

	outf.finalize();

	std::cout << "processing time: " << processingTime.count() << " s"
			  << std::endl;
}
