#include <string>
#include <fstream>
#include <cstring>
#include <iostream>
#include <vector>

#include "r8brain-free-src/CDSPResampler.h"


class CWaveFile
{
protected:
	const char *MagicRiff = "RIFF";
	const char *MagicWave = "WAVE";
	const char *MagicFmt = "fmt\x20";
	const char *MagicData = "data";

	const uint16_t FormatPcm = UINT16_C( 1 );

	std::fstream fs;

	uint64_t SampleNegBase;
	uint64_t SampleNegMask;

public:
	double SampleRate;
	size_t ChannelCount;
	size_t SampleCount;
	uint16_t BitsPerSample;
	uint8_t BytesPerSample;

protected:
	uint64_t getRawSample( std::vector< uint8_t > &buffer, size_t index )
	{
		uint64_t result = UINT64_C( 0 );

		for ( size_t i = 0; i < BytesPerSample; i++ )
		{
			result |= static_cast< uint64_t >( buffer[ index + i ] )
				<< ( i * 8 );
		}

		return result;
	}

	void setRawSample(
		std::vector< uint8_t > &buffer, size_t index, uint64_t sample )
	{

		for ( size_t i = 0; i < BytesPerSample; i++ )
		{
			buffer[ index + i ] = ( sample >> ( i * 8 ) ) & 0xff;
		}
	}

	std::vector< uint8_t > readChunk( const char *magic, size_t minLength )
	{
		uint8_t bufHeader[ 8 ];
		size_t chunkSize;

		while ( true )
		{
			fs.read(
				reinterpret_cast< char * >( bufHeader ), sizeof bufHeader );

			chunkSize = bufHeader[ 4 ] | ( bufHeader[ 5 ] << 8 ) |
				( bufHeader[ 6 ] << 16 ) | ( bufHeader[ 7 ] << 24 );

			if ( memcmp( magic, bufHeader, 4 ) == 0 )
			{
				break;
			}

			fs.ignore( chunkSize );
		}

		std::vector< uint8_t > buffer( chunkSize );
		fs.read( reinterpret_cast< char * >(
					 const_cast< uint8_t * >( buffer.data() ) ),
			buffer.capacity() );

		return buffer;
	}

	uint32_t toValue32Le( const uint8_t *buffer )
	{
		return ( buffer[ 0 ] | ( buffer[ 1 ] << 8 ) | ( buffer[ 2 ] << 16 ) |
			( buffer[ 3 ] << 24 ) );
	}

	uint16_t toValue16Le( const uint8_t *buffer )
	{
		return ( buffer[ 0 ] | ( buffer[ 1 ] << 8 ) );
	}

	void toBin16Le( char *buffer, uint16_t value )
	{
		buffer[ 0 ] = value & 0xff;
		buffer[ 1 ] = ( value >> 8 ) & 0xff;
	}

	void toBin32Le( char *buffer, uint32_t value )
	{
		buffer[ 0 ] = value & 0xff;
		buffer[ 1 ] = ( value >> 8 ) & 0xff;
		buffer[ 2 ] = ( value >> 16 ) & 0xff;
		buffer[ 3 ] = ( value >> 24 ) & 0xff;
	}

public:
	CWaveFile()
		: SampleCount( UINT64_C( 0 ) )
	{
	}

	virtual ~CWaveFile()
	{
		if ( fs )
		{
			fs.close();
		}
	}

	void loadFile( const std::string &name )
	{
		fs.open( name, std::ios_base::in | std::ios_base::binary );

		if ( !fs )
		{
			return;
		}

		size_t rootChunkSize = UINT64_C( 0 );

		{
			uint8_t buf[ 12 ];
			fs.read( reinterpret_cast< char * >( buf ), sizeof buf );

			if ( memcmp( MagicRiff, buf, 4 ) != 0 )
			{
				return;
			}

			if ( memcmp( MagicWave, buf + 8, 4 ) != 0 )
			{
				return;
			}

			rootChunkSize = toValue32Le( buf + 4 );
		}

		// std::cout << rootChunkSize << std::endl;

		{
			auto buf = readChunk( MagicFmt, 24 - 8 );

			uint16_t format = toValue16Le( buf.data() );

			if ( format != FormatPcm )
			{
				return;
			}

			size_t index = 2;
			ChannelCount = toValue16Le( buf.data() + index );
			index += 2;
			SampleRate =
				static_cast< double >( toValue32Le( buf.data() + index ) );

			index += 4 + 4 + 2;

			BitsPerSample = toValue16Le( buf.data() + index );
			BytesPerSample = BitsPerSample / 8;

			SampleNegBase = UINT64_C( 1 ) << BitsPerSample;
			SampleNegMask = UINT64_C( 1 ) << ( BitsPerSample - 1 );
		}

		{
			uint8_t buf[ 8 ];
			fs.read( reinterpret_cast< char * >( buf ), sizeof buf );

			if ( memcmp( MagicData, buf, 4 ) != 0 )
			{
				return;
			}

			size_t chunkSize = toValue32Le( buf + 4 );
			SampleCount = chunkSize / ( ChannelCount * BytesPerSample );
		}
	}

	void saveFile( const std::string &name )
	{
		fs.open( name,
			std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );

		char buf[ 44 ];
		fs.write( buf, sizeof buf );
	}

	void inheritFormat( CWaveFile &wf )
	{
		SampleRate = wf.SampleRate;
		ChannelCount = wf.ChannelCount;
		BitsPerSample = wf.BitsPerSample;
		BytesPerSample = wf.BytesPerSample;

		SampleNegBase = wf.SampleNegBase;
		SampleNegMask = wf.SampleNegMask;
	}

	void readData(
		r8b::CFixedBuffer< double > *buffers, int bufferSize, int &samplesRead )
	{

		std::vector< uint8_t > buffer(
			bufferSize * ChannelCount * BytesPerSample );

		fs.read( reinterpret_cast< char * >(
					 const_cast< uint8_t * >( buffer.data() ) ),
			buffer.capacity() );

		auto samplesCount = fs.gcount() / ( ChannelCount * BytesPerSample );
		samplesRead = samplesCount < bufferSize ? -1 : samplesCount;

		size_t bufI = UINT64_C( 0 );

		for ( size_t sampleI = 0; sampleI < samplesCount; sampleI++ )
		{
			for ( size_t channelI = 0; channelI < ChannelCount; channelI++ )
			{
				uint64_t sampleRaw = getRawSample( buffer, bufI );
				bufI += BytesPerSample;

				double sample = ( sampleRaw & SampleNegMask ) != 0
					? -( static_cast< double >( SampleNegBase ) - sampleRaw )
					: sampleRaw;

				buffers[ channelI ][ sampleI ] = sample;
			}
		}
	}

	void writeData( double *data[], int sampleCount )
	{
		SampleCount += sampleCount;

		std::vector< uint8_t > buffer(
			sampleCount * ChannelCount * BytesPerSample );

		size_t bufI = UINT64_C( 0 );

		for ( size_t sampleI = 0; sampleI < sampleCount; sampleI++ )
		{
			for ( size_t channelI = 0; channelI < ChannelCount; channelI++ )
			{
				auto sample = data[ channelI ][ sampleI ];
				uint64_t sampleRaw = sample >= 0
					? sample
					: static_cast< double >( SampleNegBase ) + sample;

				setRawSample( buffer, bufI, sampleRaw );

				bufI += BytesPerSample;
			}
		}

		fs.write( reinterpret_cast< const char * >( buffer.data() ),
			buffer.capacity() );
	}

	void finalize()
	{
		fs.seekp( 0 );
		fs.write( MagicRiff, 4 );

		size_t dataSize = SampleCount * ChannelCount * BytesPerSample;

		{
			size_t chunkSize = dataSize + 4 + 24 + 8;

			char buf[ 4 ];
			toBin32Le( buf, chunkSize );

			fs.write( buf, sizeof buf );
		}

		fs.write( MagicWave, 4 );

		fs.write( MagicFmt, 4 );

		{
			char buf[ 20 ];
			size_t index = 0;
			toBin32Le( buf, 16 );
			index += 4;
			toBin16Le( buf + index, FormatPcm );
			index += 2;
			toBin16Le( buf + index, ChannelCount );
			index += 2;
			toBin32Le( buf + index, SampleRate );
			index += 4;
			toBin32Le(
				buf + index, SampleRate * ChannelCount * BytesPerSample );
			index += 4;
			toBin16Le( buf + index, ChannelCount * BytesPerSample );
			index += 2;
			toBin16Le( buf + index, BitsPerSample );

			fs.write( buf, sizeof buf );
		}

		fs.write( MagicData, 4 );

		{
			char buf[ 4 ];
			toBin32Le( buf, dataSize );
			fs.write( buf, sizeof buf );
		}

		fs.flush();
	}
};
