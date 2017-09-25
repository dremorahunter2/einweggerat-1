

#include "bind_list.h"

#include <assert.h>

#include <windows.h>
#include <cmath>
#include "abstract_file.h"


class bind_list_i : public bind_list
{
	guid_container * guids;

	unsigned char joy;


	int speed;

	bool paused;

	int frames_until_paused;
	int frames_until_run;

	


	std::vector< bind > list;

	/*CRITICAL_SECTION sync;

	inline void lock()
	{
		EnterCriticalSection( & sync );
	}

	inline void unlock()
	{
		LeaveCriticalSection( & sync );
	}*/

#define lock()
#define unlock()

	void press( unsigned which, int16_t value )
	{
		assert(which < list.size());
		list[which].status = true;
		list[which].value = value;
	}

	void release( unsigned which, int16_t value )
	{
		assert(which < list.size());
		list[which].status = false;
		list[which].value = value;
	}

public:
	virtual bool getbutton(int which, int16_t & value, int & retro_id, bool & isanalog)
	{
		assert(which < list.size());
		const bind & b = list[which];
		value = b.value;
		retro_id = b.retro_id;
		isanalog = b.isxinput_y;
		return b.status;
	}


	bind_list_i( guid_container * _guids ) : guids( _guids )
	{
		reset();
		//InitializeCriticalSection( & sync );
	}

	virtual ~bind_list_i()
	{
		clear();
		//DeleteCriticalSection( & sync );
	}

	virtual bind_list * copy()
	{
		lock();
			bind_list * out = new bind_list_i( guids );

			std::vector< bind >::iterator it;
			for ( it = list.begin(); it < list.end(); ++it )
			{
				out->add( it->e, it->action,it->description,it->retro_id );
			}
		unlock();

		return out;
	}

	virtual void add( const dinput::di_event & e, unsigned action,TCHAR *description, unsigned retro_id )
	{
		lock();
			if ( e.type == dinput::di_event::ev_joy )
			{
				GUID guid;
				if ( guids->get_guid( e.joy.serial, guid ) )
					guids->add( guid );
			}
			bind b = { 0 };
			b.e = e;
			b.action = action;
			b.status = false;
			b.value = 0;
			
			b.retro_id = retro_id;
			lstrcpy(b.description, description);
			b.isxinput_y = false;
			if (e.type == dinput::di_event::ev_xinput)
			{
				if (e.xinput.which & 1 && e.xinput.type == dinput::di_event::xinput_axis)
				{
					b.isxinput_y = true;
				}
			}
			list.push_back( b );



		unlock();
	}

	virtual unsigned get_count()
	{
		lock();
			unsigned count = list.end() - list.begin();
		unlock();

		return count;
	}

	virtual void get( unsigned index, dinput::di_event & e, unsigned & action, TCHAR * description, unsigned & retro_id )
	{
		lock();
			assert( index < get_count() );

			const bind & b = list[ index ];

			e = b.e;
			action = b.action;
			retro_id = b.retro_id;
			lstrcpy(description, (TCHAR*)b.description);
		unlock();
	}

	virtual void remove( unsigned index )
	{
		lock();
			assert( index < get_count() );

			std::vector< bind >::iterator it = list.begin() + index;
			GUID guid;

			if ( it->e.type == dinput::di_event::ev_joy )
			{
				if ( guids->get_guid( it->e.joy.serial, guid ) )
					guids->remove( guid );
			}

			list.erase( it );
		unlock();
	}

	virtual void clear()
	{
		lock();
			std::vector< bind >::iterator it;
			GUID guid;
			for ( it = list.begin(); it < list.end(); ++it )
			{
				if ( it->e.type == dinput::di_event::ev_joy )
				{
					if ( guids->get_guid( it->e.joy.serial, guid ) )
						guids->remove( guid );
				}
			}
			list.clear();
		unlock();
	}

	virtual void replace(unsigned index, const dinput::di_event & e, unsigned action, TCHAR* description, unsigned retro_id)
	{
		lock();
		std::vector< bind > list2;
		list2 = list;
		std::vector< bind >::iterator it;
		GUID guid;
		for (it = list.begin(); it < list.end(); ++it)
		{
			if (it->e.type == dinput::di_event::ev_joy)
			{
				if (guids->get_guid(it->e.joy.serial, guid))
					guids->remove(guid);
			}
		}
		list.clear();

		for (int i = 0; i < (list2.end() - list2.begin()); i++)
		{
			bind b;
			if (i == index)
			{
				b.e = e;
				b.action = action;
				b.retro_id = retro_id;
				b.isxinput_y = false;
				if (e.type == dinput::di_event::ev_xinput)
				{
					if (e.xinput.which & 1 && e.xinput.type == dinput::di_event::xinput_axis)
					{
						b.isxinput_y = true;
					}
				}
				lstrcpy(b.description, description);
			}
			else
			{
				b.e = list2.at(i).e;
				b.action = list2.at(i).action;
				b.retro_id = list2.at(i).retro_id;
				b.isxinput_y = list2.at(i).isxinput_y;
				lstrcpy(b.description, list2.at(i).description);
			}
			if (guids->get_guid(b.e.joy.serial, guid))
				guids->add(guid);
			list.push_back(b);
		}
		list2.clear();

		unlock();
	}

	virtual const char * load( Data_Reader & in )
	{
		const char * err = "Invalid input config file";

		lock();
			clear();
			reset();

			do
			{
				unsigned n;
				err = in.read( & n, sizeof( n ) ); if ( err ) break;

				for ( unsigned i = 0; i < n; ++i )
				{
					bind b = { 0 };

					err = in.read( & b.action, sizeof( b.action ) ); if ( err ) break;
					err = in.read( & b.e.type, sizeof( b.e.type ) ); if ( err ) break;
					err = in.read( &b.description, sizeof(b.description)); if (err) break;
					err = in.read( &b.retro_id, sizeof(b.retro_id)); if (err) break;
					err = in.read(&b.isxinput_y, sizeof(b.isxinput_y)); if (err) break;

					if ( b.e.type == dinput::di_event::ev_none )
					{
						err = in.read( & b.e.key.which, sizeof( b.e.key.which ) ); if ( err ) break;
						list.push_back( b );
					}

					else if (b.e.type == dinput::di_event::ev_key)
					{
						err = in.read(&b.e.key.which, sizeof(b.e.key.which)); if (err) break;
						list.push_back(b);
					}

					else if ( b.e.type == dinput::di_event::ev_joy )
					{
						GUID guid;
						err = in.read( & guid, sizeof( guid ) ); if ( err ) break;
						err = in.read( & b.e.joy.type, sizeof( b.e.joy.type ) ); if ( err ) break;
						err = in.read( & b.e.joy.which, sizeof( b.e.joy.which ) ); if ( err ) break;
						if ( b.e.joy.type == dinput::di_event::joy_axis )
						{
							err = in.read( & b.e.joy.axis, sizeof( b.e.joy.axis ) ); if ( err ) break;
						}
						else if ( b.e.joy.type == dinput::di_event::joy_button )
						{
						}
						else if ( b.e.joy.type == dinput::di_event::joy_pov )
						{
							err = in.read( & b.e.joy.pov_angle, sizeof( b.e.joy.pov_angle ) ); if ( err ) break;
						}
						else break;
						b.e.joy.serial = guids->add( guid );
						list.push_back( b );
					}
					else if ( b.e.type == dinput::di_event::ev_xinput )
					{
						err = in.read( & b.e.xinput.index, sizeof( b.e.xinput.index ) ); if ( err ) break;
						err = in.read( & b.e.xinput.type, sizeof( b.e.xinput.type ) ); if ( err ) break;
						err = in.read( & b.e.xinput.which, sizeof( b.e.xinput.which ) ); if ( err ) break;
						if ( b.e.xinput.type == dinput::di_event::xinput_axis )
						{
							err = in.read( & b.e.xinput.axis, sizeof( b.e.xinput.axis ) ); if ( err ) break;
						}

						list.push_back( b );
					}
				}

				err = 0;
			}
			while ( 0 );

		unlock();

		return err;
	}

	virtual const char * save( Data_Writer & out )
	{
		const char * err;

		lock();
			do
			{
				unsigned n = get_count();
				err = out.write( & n, sizeof( n ) ); if ( err ) break;

				std::vector< bind >::iterator it;
				for ( it = list.begin(); it < list.end(); ++it )
				{

					err = out.write( & it->action, sizeof( it->action ) ); if ( err ) break;
					err = out.write( & it->e.type, sizeof( it->e.type ) ); if ( err ) break;
					err = out.write(&it->description, sizeof(it->description)); if (err) break;
					err = out.write(&it->retro_id, sizeof(it->retro_id)); if (err) break;
					err = out.write(&it->isxinput_y, sizeof(it->isxinput_y)); if (err) break;


					if (it->e.type == dinput::di_event::ev_none)
					{
						err = out.write(&it->e.key.which, sizeof(it->e.key.which)); if (err) break;
					}


					if ( it->e.type == dinput::di_event::ev_key )
					{
						err = out.write( & it->e.key.which, sizeof( it->e.key.which ) ); if ( err ) break;
					}
					else if ( it->e.type == dinput::di_event::ev_joy )
					{
						GUID guid;
						if ( ! guids->get_guid( it->e.joy.serial, guid ) ) { err = "GUID missing"; break; }
						err = out.write( & guid, sizeof( guid ) ); if ( err ) break;
						err = out.write( & it->e.joy.type, sizeof( it->e.joy.type ) ); if ( err ) break;
						err = out.write( & it->e.joy.which, sizeof( it->e.joy.which ) ); if ( err ) break;
						if ( it->e.joy.type == dinput::di_event::joy_axis )
						{
							err = out.write( & it->e.joy.axis, sizeof( it->e.joy.axis ) ); if ( err ) break;
						}
						else if ( it->e.joy.type == dinput::di_event::joy_pov )
						{
							err = out.write( & it->e.joy.pov_angle, sizeof( it->e.joy.pov_angle ) ); if ( err ) break;
						}
					}
					else if ( it->e.type == dinput::di_event::ev_xinput )
					{
						err = out.write( & it->e.xinput.index, sizeof( it->e.xinput.index ) ); if ( err ) break;
						err = out.write( & it->e.xinput.type, sizeof( it->e.xinput.type ) ); if ( err ) break;
						err = out.write( & it->e.xinput.which, sizeof( it->e.xinput.which ) ); if ( err ) break;
						if ( it->e.xinput.type == dinput::di_event::xinput_axis )
						{
							err = out.write( & it->e.xinput.axis, sizeof( it->e.xinput.axis ) ); if ( err ) break;
						}
					}
				}
			}
			while ( 0 );

		unlock();

		return err;
	}

	virtual void process( std::vector< dinput::di_event > & events )
	{
		lock();
			std::vector< dinput::di_event >::iterator it;
			for ( it = events.begin(); it < events.end(); ++it )
			{
				std::vector< bind >::iterator itb;
				if ( it->type == dinput::di_event::ev_key ||
					( it->type == dinput::di_event::ev_joy &&
					  it->joy.type == dinput::di_event::joy_button ) ||
					( it->type == dinput::di_event::ev_xinput &&
					  it->xinput.type == dinput::di_event::xinput_button ) )
				{
					// two state
					if ( it->type == dinput::di_event::ev_key )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_key &&
								itb->e.key.which == it->key.which )
							{
								if ( it->key.type == dinput::di_event::key_down ) press( itb->action, 65535 );
								else release( itb->action, 0 );
							}
						}
					}
					else
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_button &&
								itb->e.joy.which == it->joy.which ) ||
								( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_button &&
								itb->e.xinput.which == it->xinput.which ) )
							{
								if ( ( it->type == dinput::di_event::ev_joy && it->joy.button == dinput::di_event::button_down ) ||
									( it->type == dinput::di_event::ev_xinput && it->xinput.button == dinput::di_event::button_down ) ) press( itb->action, 65535 );
								else release( itb->action, 0 );
							}
						}
					}
				}
				else if ( it->type == dinput::di_event::ev_joy )
				{
					// mutually exclusive in class set
					if ( it->joy.type == dinput::di_event::joy_axis )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_axis &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.axis != itb->e.joy.axis && it->joy.axis) release( itb->action, it->joy.value );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_axis &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.axis == itb->e.joy.axis && it->joy.axis) press(itb->action, it->joy.value);
							}
						}
					}
					else if ( it->joy.type == dinput::di_event::joy_pov )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_pov &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.pov_angle != itb->e.joy.pov_angle ) release( itb->action, 0 );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_pov &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.pov_angle == itb->e.joy.pov_angle ) press( itb->action, 65535 );
							}
						}
					}
				}
				else if ( it->type == dinput::di_event::ev_xinput )
				{
					// mutually exclusive in class set
					if ( it->xinput.type == dinput::di_event::xinput_axis )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_axis &&
								itb->e.xinput.which == it->xinput.which )
							{
								{
									if (it->xinput.axis != itb->e.xinput.axis)
									{
										//if(it->xinput.which & 1)release(itb->action, (it->joy.axis == dinput::di_event::axis_negative) ? (abs((int)it->joy.value)) : -it->joy.value);
										 release(itb->action, it->joy.value);
									}
								}

								
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if (itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_axis &&
								itb->e.xinput.which == it->xinput.which)
							{
								if (it->xinput.axis == itb->e.xinput.axis)
								{
									//if (it->xinput.which & 1)press(itb->action, (it->joy.axis == dinput::di_event::axis_negative) ? (abs((int)it->joy.value)) : -it->joy.value);
									press(itb->action, it->joy.value);
								}

							}
						}
					}
					else if ( it->xinput.type == dinput::di_event::xinput_trigger )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_trigger &&
								itb->e.xinput.which == it->xinput.which )
							{
								if ( it->xinput.button == dinput::di_event::button_down ) press( itb->action, it->xinput.value * 257 );
								else release( itb->action, it->xinput.value * 257 );
							}
						}
					}
				}
			}
		unlock();
	}


	

	virtual unsigned read()
	{
		return joy;
	}

	virtual void strobe( unsigned joy )
	{
	}

	virtual void update()
	{
		if ( frames_until_paused && !--frames_until_paused ) paused = true;
		if ( frames_until_run && !--frames_until_run ) paused = false;
	}

	virtual int get_speed() const
	{
		return paused ? 0 : speed;
	}

	virtual void set_speed( int speed )
	{
		this->speed = speed;
	}

	virtual void set_paused( bool paused )
	{
		this->paused = paused;
	}

	virtual void reset()
	{
		joy= 0;
		speed = 1;
		paused = false;
		frames_until_paused = 0;
		frames_until_run = 0;
		/*rapid_toggle[ 0 ] = false;
		rapid_toggle[ 1 ] = false;*/
	}
};

bind_list * create_bind_list( guid_container * guids )
{
	return new bind_list_i( guids );
}
