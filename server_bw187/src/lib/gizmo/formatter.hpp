/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FORMATER_HPP_
#define FORMATER_HPP_

/**
 *	This templatised class formats a label string.
 */
template<class T>
class LabelFormatter
{
public:
	virtual const std::string& format( const T& value ) = 0;
protected:
	std::string label_;
};


/**
 *	This class labels a float as a distance.
 */
class DistanceFormatter : public LabelFormatter<float>
{
public:
	const std::string& format( const float& value )
	{
		char buf[64];

		if ( fabsf( value ) < 1.f )
		{
			sprintf( buf, "%0.0fcm.", value * 100.f );
		}
		else if ( fabsf( value ) < 10.f )
		{
			sprintf( buf, "%0.2fm.", value );
		}
		else if ( fabsf( value ) < 1000.f )
		{
			sprintf( buf, "%0.1fm.", value );
		}
		else
		{
			sprintf( buf, "%0.3fkm.", value / 1000.f );
		}

		label_ = buf;
		return label_;
	}

	static DistanceFormatter s_def;
};


/**
 *	This class labels a radians float as a degrees angle.
 */
class AngleFormatter : public LabelFormatter<float>
{
public:
	const std::string& format( const float& value )
	{
		char buf[64];

		sprintf( buf, "%0.1fdeg.", value);

		label_ = buf;
		return label_;
	}

	static AngleFormatter s_def;
};

class SimpleFormatter : public LabelFormatter<float>
{
public:
	const std::string& format( const float& value )
	{
		char buf[64];

		sprintf( buf, "%0.2f", value );

		label_ = buf;
		return label_;
	}

	static SimpleFormatter s_def;
};

#endif