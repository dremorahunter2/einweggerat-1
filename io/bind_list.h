#ifndef _bind_list_h_
#define _bind_list_h_

#include "dinput.h"
#include "guid_container.h"

class Data_Reader;
class Data_Writer;

class bind_list
{
public:
	enum
	{
		bind_pad_0_x,
		bind_pad_0_y,
		bind_pad_0_select,
		bind_pad_0_start,
		bind_pad_0_up,
		bind_pad_0_down,
		bind_pad_0_left,
		bind_pad_0_right,
	};

	virtual ~bind_list() {}

	virtual bind_list * copy() = 0;

	// list manipulation
	virtual void add( const dinput::di_event &, unsigned action, TCHAR* description ) = 0;

	virtual void replace(unsigned index, const dinput::di_event & e, unsigned action, TCHAR* description) = 0;

	virtual unsigned get_count( ) = 0;

	virtual bool getbutton(int which, int & value) = 0;

	virtual void get( unsigned index, dinput::di_event &, unsigned & action , TCHAR * description) = 0;

	virtual void remove( unsigned index ) = 0;

	virtual void clear( ) = 0;

	// configuration file
	virtual const char * load( Data_Reader & ) = 0;

	virtual const char * save( Data_Writer & ) = 0;

	// input handling
	virtual void process( std::vector< dinput::di_event > & ) = 0;
	
	virtual unsigned read( ) = 0;

	virtual void strobe( unsigned ) = 0;

	virtual int get_speed() const = 0;

	virtual void set_speed( int ) = 0;

	virtual void set_paused( bool ) = 0;

	virtual void reset() = 0;

	virtual void update() = 0;
};

bind_list * create_bind_list( guid_container * );

#endif