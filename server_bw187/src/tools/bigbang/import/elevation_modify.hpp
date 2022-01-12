/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_MODIFY_HPP
#define ELEVATION_MODIFY_HPP


/**
 *  This object gives exclusive access to chunks while it is in scope.
 */
class ElevationModify
{
public:
    explicit ElevationModify(bool readOnly = false);

    ~ElevationModify();

    operator bool() const;

private:
    ElevationModify(ElevationModify const &);
    ElevationModify &operator=(ElevationModify const &);

private:
    bool            ok_;
    bool            readOnly_;
};


#endif // ELEVATION_MODIFY_HPP
