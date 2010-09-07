#include <allegro5/allegro5.h>

#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>

#include <stdio.h>

#include "t3f.h"

/* display data */
int t3f_virtual_display_width = 0;
int t3f_virtual_display_height = 0;
float t3f_mouse_scale_x = 1.0;
float t3f_mouse_scale_y = 1.0;

/* keyboard data */
bool t3f_key[ALLEGRO_KEY_MAX] = {false};
char t3f_key_buffer[T3F_KEY_BUFFER_MAX] = {0};
int t3f_key_buffer_keys = 0;

/* mouse data */
int t3f_mouse_x = 0;
int t3f_mouse_y = 0;
int t3f_mouse_z = 0;
int t3f_mouse_dx = 0;
int t3f_mouse_dy = 0;
int t3f_mouse_dz = 0;
bool t3f_mouse_button[16] = {0};

/* joystick data */
ALLEGRO_JOYSTICK * t3f_joystick[T3F_MAX_JOYSTICKS] = {NULL};
ALLEGRO_JOYSTICK_STATE t3f_joystick_state[T3F_MAX_JOYSTICKS];

ALLEGRO_TRANSFORM t3f_base_transform;
ALLEGRO_TRANSFORM t3f_current_transform;

/* blender data */
ALLEGRO_STATE t3f_state_stack[T3F_MAX_STACK];
int t3f_state_stack_size = 0;

bool t3f_quit = false;
int t3f_requested_flags = 0;
int t3f_flags = 0;

void (*t3f_logic_proc)() = NULL;
void (*t3f_render_proc)() = NULL;
void (*t3f_event_proc)() = NULL;

ALLEGRO_DISPLAY * t3f_display = NULL;
ALLEGRO_TIMER * t3f_timer = NULL;
ALLEGRO_EVENT_QUEUE * t3f_queue = NULL;
char t3f_window_title[1024] = {0};

ALLEGRO_CONFIG * t3f_config = NULL;
ALLEGRO_PATH * t3f_data_path = NULL;
ALLEGRO_PATH * t3f_config_path = NULL;
static char t3f_config_filename[1024] = {0};
static char t3f_return_filename[1024] = {0};

/* creates directory structure that leads to 'final' */
void t3f_setup_directories(ALLEGRO_PATH * final)
{
	ALLEGRO_PATH * working_path[16] = {NULL};
	int working_paths = 0;
	const char * cpath = NULL;
	int i;
	
	/* find first directory that exists */
	working_path[0] = al_clone_path(final);
	working_paths = 1;
	while(!al_is_path_present(working_path[working_paths - 1]) && working_paths < 16)
	{
		working_path[working_paths] = al_clone_path(working_path[working_paths - 1]);
		al_drop_path_tail(working_path[working_paths]);
		working_paths++;
	}
	
	/* iterate through working_path[] and make each directory */
	for(i = working_paths - 1; i >= 0; i--)
	{
		cpath = al_path_cstr(working_path[i], '/');
//		printf("make directory: %s\n", cpath);
		al_make_directory(cpath);
	}
	for(i = 0; i < working_paths; i++)
	{
		al_destroy_path(working_path[i]);
	}
}

/* this gets Allegro ready */
int t3f_initialize(int w, int h, double fps, char * name, void (*logic_proc)(), void (*render_proc)(), int flags)
{
	int i;
	ALLEGRO_PATH * temp_path = NULL;
	
	/* initialize Allegro */
	if(!al_init())
	{
		printf("Allegro failed to initialize!\n");
		return 0;
	}
	al_set_app_name(name);
	#ifdef T3F_COMPANY
		al_set_org_name(T3F_COMPANY);
	#endif
	
	/* set up application path */
	t3f_config_path = al_get_standard_path(ALLEGRO_USER_SETTINGS_PATH);
	t3f_data_path = al_get_standard_path(ALLEGRO_USER_DATA_PATH);
	t3f_setup_directories(t3f_config_path);
	t3f_setup_directories(t3f_data_path);
	
	/* set up configuration file */
	temp_path = al_clone_path(t3f_config_path);
	al_set_path_filename(temp_path, "settings.ini");
	strcpy(t3f_config_filename, al_path_cstr(temp_path, '/'));
	al_destroy_path(temp_path);
	t3f_config = al_load_config_file(t3f_config_filename);
	if(!t3f_config)
	{
		t3f_config = al_create_config();
	}
	
	if(!al_init_image_addon())
	{
		printf("Failed to initialize Image I/O module!\n");
		return 0;
	}
	al_init_font_addon();
	if(flags & T3F_USE_SOUND)
	{
		if(!al_install_audio())
		{
			printf("Could not init sound!\n");
		}
		else if(!al_reserve_samples(16))
		{
			printf("Could not set up voice and mixer.\n");
		}
		else
		{
			t3f_flags |= T3F_USE_SOUND;
		}
		al_init_acodec_addon();
	}
	if(flags & T3F_USE_KEYBOARD)
	{
		if(!al_install_keyboard())
		{
			printf("Failed to initialize keyboard!\n");
		}
		else
		{
			t3f_flags |= T3F_USE_KEYBOARD;
		}
	}
	if(flags & T3F_USE_MOUSE)
	{
		if(!al_install_mouse())
		{
			printf("Failed to initialize mouse!\n");
		}
		else
		{
			t3f_flags |= T3F_USE_MOUSE;
		}
	}
	if(flags & T3F_USE_JOYSTICK)
	{
		if(!al_install_joystick())
		{
			printf("Failed to initialize joystick!\n");
		}
		else
		{
			t3f_flags |= T3F_USE_JOYSTICK;
		}
	}
	al_init_primitives_addon();
	
	/* if we are using console (for a server, for instance) don't create display */
	strcpy(t3f_window_title, name);
	if(flags & T3F_USE_CONSOLE)
	{
		t3f_flags |= T3F_USE_CONSOLE;
	}
	else
	{
		if(!t3f_set_gfx_mode(w, h, flags))
		{
			return 0;
		}
	}
	
	t3f_timer = al_create_timer(1.000 / fps);
	if(!t3f_timer)
	{
		printf("Failed to create timer!\n");
		return 0;
	}
	
	t3f_queue = al_create_event_queue();
	if(!t3f_queue)
	{
		printf("Failed to create event queue!\n");
		return 0;
	}
	
	if(t3f_flags & T3F_USE_KEYBOARD)
	{
		al_register_event_source(t3f_queue, al_get_keyboard_event_source());
	}
	if(t3f_flags & T3F_USE_MOUSE)
	{
		al_register_event_source(t3f_queue, al_get_mouse_event_source());
	}
	if(t3f_flags & T3F_USE_JOYSTICK)
	{
		for(i = 0; i < T3F_MAX_JOYSTICKS; i++)
		{
			t3f_joystick[i] = al_get_joystick(i);
			if(t3f_joystick[i])
			{
				al_register_event_source(t3f_queue, al_get_joystick_event_source(t3f_joystick[i]));
			}
		}
	}
	if(!(t3f_flags & T3F_USE_CONSOLE))
	{
		al_register_event_source(t3f_queue, al_get_display_event_source(t3f_display));
	}
	al_register_event_source(t3f_queue, al_get_timer_event_source(t3f_timer));
	
	t3f_logic_proc = logic_proc;
	t3f_render_proc = render_proc;
	
	return 1;
}

int t3f_set_gfx_mode(int w, int h, int flags)
{
	const char * cvalue = NULL;
	const char * cvalue2 = NULL;
	char val[128] = {0};
	int dflags = 0;
	int dw, dh;
	if(t3f_display)
	{
		/* if we are switching from window to full screen, create new display */
//		al_destroy_display(t3f_display);
		if(flags & T3F_USE_FULLSCREEN)
		{
			al_toggle_display_flag(t3f_display, ALLEGRO_FULLSCREEN_WINDOW, true);
//			al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW);
		}
		else
		{
			al_toggle_display_flag(t3f_display, ALLEGRO_FULLSCREEN_WINDOW, false);
//			al_set_new_display_flags(ALLEGRO_WINDOWED);
		}
//		t3f_display = al_create_display(w, h);
//		if(!t3f_display)
//		{
//			printf("Failed to create display!\n");
//			return 0;
//		}
		sprintf(val, "%d", w);
		al_set_config_value(t3f_config, "T3F", "display_width", val);
		sprintf(val, "%d", h);
		al_set_config_value(t3f_config, "T3F", "display_height", val);
		al_build_transform(&t3f_base_transform, 0.0, 0.0, (float)al_get_display_width(t3f_display) / (float)t3f_virtual_display_width, (float)al_get_display_height(t3f_display) / (float)t3f_virtual_display_height, 0.0);
		t3f_mouse_scale_x = (float)t3f_virtual_display_width / (float)al_get_display_width(t3f_display);
		t3f_mouse_scale_y = (float)t3f_virtual_display_height / (float)al_get_display_height(t3f_display);
		al_set_window_title(t3f_display, t3f_window_title);
	}
	
	/* first time creating display */
	else
	{
		if(t3f_flags & T3F_USE_FULLSCREEN)
		{
			t3f_flags ^= T3F_USE_FULLSCREEN;
		}
		if(t3f_flags & T3F_RESIZABLE)
		{
			t3f_flags ^= T3F_RESIZABLE;
		}
		if(t3f_flags & T3F_FORCE_ASPECT)
		{
			t3f_flags ^= T3F_FORCE_ASPECT;
		}
		
		/* if we are using console (for a server, for instance) don't create display */
		if(flags & T3F_USE_CONSOLE)
		{
			t3f_flags |= T3F_USE_CONSOLE;
		}
		else
		{
			cvalue = al_get_config_value(t3f_config, "T3F", "force_fullscreen");
			cvalue2 = al_get_config_value(t3f_config, "T3F", "force_window");
			if((flags & T3F_USE_FULLSCREEN || (cvalue && !stricmp(cvalue, "true"))) && !(cvalue2 && !stricmp(cvalue2, "true")))
			{
				dflags |= ALLEGRO_FULLSCREEN_WINDOW;
				t3f_flags |= T3F_USE_FULLSCREEN;
			}
			if(flags & T3F_RESIZABLE)
			{
				dflags |= ALLEGRO_RESIZABLE;
				t3f_flags |= T3F_RESIZABLE;
				t3f_flags |= (flags & T3F_FORCE_ASPECT);
			}
			al_set_new_display_flags(dflags);
			cvalue = al_get_config_value(t3f_config, "T3F", "display_width");
			cvalue2 = al_get_config_value(t3f_config, "T3F", "display_height");
			if(cvalue && cvalue2)
			{
				dw = atoi(cvalue);
				dh = atoi(cvalue2);
			}
			else
			{
				dw = w;
				dh = h;
			}
			t3f_display = al_create_display(dw, dh);
			if(!t3f_display)
			{
				printf("Failed to create display!\n");
				return 0;
			}
			t3f_virtual_display_width = w;
			t3f_virtual_display_height = h;
			al_build_transform(&t3f_base_transform, 0.0, 0.0, (float)al_get_display_width(t3f_display) / (float)t3f_virtual_display_width, (float)al_get_display_height(t3f_display) / (float)t3f_virtual_display_height, 0.0);
			t3f_mouse_scale_x = (float)t3f_virtual_display_width / (float)al_get_display_width(t3f_display);
			t3f_mouse_scale_y = (float)t3f_virtual_display_height / (float)al_get_display_height(t3f_display);
			al_set_window_title(t3f_display, t3f_window_title);
		}
	}
	return 1;
}

void t3f_exit(void)
{
	al_save_config_file(t3f_config_filename, t3f_config);
	t3f_quit = true;
}

float t3f_distance(float x1, float y1, float x2, float y2)
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	return sqrt(dx * dx + dy * dy);
}

#define T3F_RS_SCALE (1.0 / (1.0 + RAND_MAX))
double t3f_drand(void)
{
	double d;
	do
	{
		d = (((rand () * T3F_RS_SCALE) + rand ()) * T3F_RS_SCALE + rand ()) * T3F_RS_SCALE;
	} while (d >= 1); /* Round off */
	return d;
}

void t3f_clear_keys(void)
{
	t3f_key_buffer_keys = 0;
}

bool t3f_add_key(char key)
{
	if(t3f_key_buffer_keys < T3F_KEY_BUFFER_MAX)
	{
		t3f_key_buffer[t3f_key_buffer_keys] = key;
		t3f_key_buffer_keys++;
		return true;
	}
	return false;
}

char t3f_read_key(int flags)
{
	char rkey = 0;
	if(t3f_key_buffer_keys > 0)
	{
		t3f_key_buffer_keys--;
		rkey = t3f_key_buffer[t3f_key_buffer_keys];
		if(flags & T3F_KEY_BUFFER_FORCE_LOWER)
		{
			if(rkey >= 'A' && rkey <= 'Z')
			{
				rkey += 'a' - 'A';
			}
		}
		else if(flags & T3F_KEY_BUFFER_FORCE_UPPER)
		{
			if(rkey >= 'a' && rkey <= 'z')
			{
				rkey -= 'a' - 'A';
			}
		}
	}
	return rkey;
}

bool t3f_push_state(int flags)
{
	if(t3f_state_stack_size < T3F_MAX_STACK)
	{
		al_store_state(&t3f_state_stack[t3f_state_stack_size], flags);
		t3f_state_stack_size++;
		return true;
	}
	return false;
}

bool t3f_pop_state(void)
{
	if(t3f_state_stack_size > 0)
	{
		al_restore_state(&t3f_state_stack[t3f_state_stack_size - 1]);
		t3f_state_stack_size--;
		return true;
	}
	return false;
}

int t3f_get_joystick_number(ALLEGRO_JOYSTICK * jp)
{
	return al_get_joystick_number(jp);
}

float t3f_fread_float(ALLEGRO_FILE * fp)
{
	char buffer[256] = {0};
	int l;
	
	l = al_fgetc(fp);
	al_fread(fp, buffer, l);
	buffer[l] = '\0';
	return atof(buffer);
//	float f;
//	al_fread(fp, &f, sizeof(float));
//	return f;
}

int t3f_fwrite_float(ALLEGRO_FILE * fp, float f)
{
	char buffer[256] = {0};
	int l;
	
	sprintf(buffer, "%f", f);
	l = strlen(buffer);
	al_fputc(fp, l);
	al_fwrite(fp, buffer, l);
//	al_fwrite(fp, &f, sizeof(float));
	return 1;
}

static void t3f_convert_grey_to_alpha(ALLEGRO_BITMAP * bitmap)
{
	int x, y;
	unsigned char ir, ig, ib, ia;
	ALLEGRO_COLOR pixel;
	ALLEGRO_STATE old_state;

	if(!al_lock_bitmap(bitmap, al_get_bitmap_format(bitmap), 0))
	{
		TRACE("al_convert_mask_to_alpha: Couldn't lock bitmap.\n");
		return;
	}

	al_store_state(&old_state, ALLEGRO_STATE_TARGET_BITMAP);
	al_set_target_bitmap(bitmap);

	for(y = 0; y < al_get_bitmap_height(bitmap); y++)
	{
		for(x = 0; x < al_get_bitmap_width(bitmap); x++)
		{
			pixel = al_get_pixel(bitmap, x, y);
			al_unmap_rgba(pixel, &ir, &ig, &ib, &ia);
			if(ir == 255 && ig == 0 && ib == 255)
			{
				pixel = al_map_rgba(255, 255, 255, 0);
				al_put_pixel(x, y, pixel);
			}
			else if(ia > 0 && !(ir == 255 && ig == 255 && ib == 0))
			{
				pixel = al_map_rgba(255, 255, 255, ir);
				al_put_pixel(x, y, pixel);
			}
		}
	}

	al_restore_state(&old_state);
	al_unlock_bitmap(bitmap);
}

ALLEGRO_FONT * t3f_load_font(const char * fn)
{
	ALLEGRO_BITMAP * fimage;
	ALLEGRO_FONT * fp;
	ALLEGRO_STATE old_state;
	int ranges[] = {32, 126};
	
	al_store_state(&old_state, ALLEGRO_STATE_NEW_BITMAP_PARAMETERS);
	al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
	fimage = al_load_bitmap(fn);
	if(!fimage)
	{
		return NULL;
	}
	t3f_convert_grey_to_alpha(fimage);
	al_restore_state(&old_state);
	fp = al_grab_font_from_bitmap(fimage, 1, ranges);
	al_destroy_bitmap(fimage);
	return fp;
}

ALLEGRO_FILE * t3f_open_file(ALLEGRO_PATH * pp, const char * fn, const char * m)
{
	ALLEGRO_PATH * tpath = al_clone_path(pp);
	al_set_path_filename(tpath, fn);
	return al_fopen(al_path_cstr(tpath, '/'), m);
}

unsigned long t3f_checksum_file(const char * fn)
{
	ALLEGRO_FILE * fp;
	unsigned long sum = 0;
	
	fp = al_fopen(fn, "rb");
	while(!al_feof(fp))
	{
		sum += al_fgetc(fp);
	}
	al_fclose(fp);
	return sum;
}

bool t3f_copy_file(const char * src, const char * dest)
{
	ALLEGRO_FILE * fsrc;
	ALLEGRO_FILE * fdest;
	char c;
	
	fsrc = al_fopen(src, "rb");
	if(!fsrc)
	{
		return false;
	}
	fdest = al_fopen(dest, "wb");
	if(!fdest)
	{
		al_fclose(fsrc);
		return false;
	}
	while(!al_feof(fsrc))
	{
		c = al_fgetc(fsrc);
		al_fputc(fdest, c);
	}
	al_fclose(fdest);
	al_fclose(fsrc);
	return true;
}

/* this function is where it's at
   somewhere in your logic code you need to set t3f_quit = true to exit */
void t3f_run(void)
{
	ALLEGRO_EVENT event;
	bool redraw = false;
	
	al_start_timer(t3f_timer);
	while(!t3f_quit)
	{
		al_wait_for_event(t3f_queue, &event);
		switch(event.type)
		{
			
			/* user pressed close button */
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
			{
				t3f_exit();
				break;
			}
			
			/* window was resized */
			case ALLEGRO_EVENT_DISPLAY_RESIZE:
			{
				char val[8] = {0};
				if(t3f_flags & T3F_FORCE_ASPECT)
				{
					if(event.display.width > al_get_display_width(t3f_display))
					{
						al_acknowledge_resize(t3f_display);
						al_resize_display(t3f_display, event.display.width, (float)event.display.width * ((float)t3f_virtual_display_height / (float)t3f_virtual_display_width));
					}
					else if(event.display.height > al_get_display_height(t3f_display))
					{
						al_acknowledge_resize(t3f_display);
						al_resize_display(t3f_display, (float)event.display.height * ((float)t3f_virtual_display_width / (float)t3f_virtual_display_height), event.display.height);
					}
					else if(event.display.width < al_get_display_width(t3f_display))
					{
						al_acknowledge_resize(t3f_display);
						al_resize_display(t3f_display, event.display.width, (float)event.display.width * ((float)t3f_virtual_display_height / (float)t3f_virtual_display_width));
					}
					else if(event.display.height < al_get_display_height(t3f_display))
					{
						al_acknowledge_resize(t3f_display);
						al_resize_display(t3f_display, (float)event.display.height * ((float)t3f_virtual_display_width / (float)t3f_virtual_display_height), event.display.height);
					}
				}
				else
				{
					al_acknowledge_resize(t3f_display);
				}
				al_build_transform(&t3f_base_transform, 0.0, 0.0, (float)al_get_display_width(t3f_display) / (float)t3f_virtual_display_width, (float)al_get_display_height(t3f_display) / (float)t3f_virtual_display_height, 0.0);
				t3f_mouse_scale_x = (float)t3f_virtual_display_width / (float)al_get_display_width(t3f_display);
				t3f_mouse_scale_y = (float)t3f_virtual_display_height / (float)al_get_display_height(t3f_display);
				sprintf(val, "%d", al_get_display_width(t3f_display));
				al_set_config_value(t3f_config, "T3F", "display_width", val);
				sprintf(val, "%d", al_get_display_height(t3f_display));
				al_set_config_value(t3f_config, "T3F", "display_height", val);
				break;
			}
			
			/* key was pressed or repeated */
			case ALLEGRO_EVENT_KEY_DOWN:
			case ALLEGRO_EVENT_KEY_REPEAT:
			{
				t3f_key[event.keyboard.keycode] = 1;
				if(event.keyboard.unichar != -1)
				{
					t3f_add_key(event.keyboard.unichar);
				}
				break;
			}
			
			/* key was released */
			case ALLEGRO_EVENT_KEY_UP:
			{
				t3f_key[event.keyboard.keycode] = 0;
				break;
			}
			
			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
			{
				t3f_mouse_button[event.mouse.button - 1] = 1;
				t3f_mouse_x = (float)event.mouse.x * t3f_mouse_scale_x;
				t3f_mouse_y = (float)event.mouse.y * t3f_mouse_scale_y;
				t3f_mouse_z = event.mouse.z;
				break;
			}
			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
			{
				t3f_mouse_button[event.mouse.button - 1] = 0;
				t3f_mouse_x = (float)event.mouse.x * t3f_mouse_scale_x;
				t3f_mouse_y = (float)event.mouse.y * t3f_mouse_scale_y;
				t3f_mouse_z = event.mouse.z;
				break;
			}
			case ALLEGRO_EVENT_MOUSE_AXES:
			{
				t3f_mouse_x = (float)event.mouse.x * t3f_mouse_scale_x;
				t3f_mouse_y = (float)event.mouse.y * t3f_mouse_scale_y;
				t3f_mouse_z = event.mouse.z;
				t3f_mouse_dx = (float)event.mouse.dx * t3f_mouse_scale_x;
				t3f_mouse_dy = (float)event.mouse.dy * t3f_mouse_scale_y;
				t3f_mouse_dz = event.mouse.dz;
				break;
			}
			
			case ALLEGRO_EVENT_JOYSTICK_AXIS:
			case ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN:
			case ALLEGRO_EVENT_JOYSTICK_BUTTON_UP:
			{
				int jn = t3f_get_joystick_number(event.joystick.source);
				if(jn >= 0)
				{
					al_get_joystick_state(t3f_joystick[jn], &t3f_joystick_state[jn]);
				}
				break;
			}
			
			/* this keeps your program running */
			case ALLEGRO_EVENT_TIMER:
			{
                t3f_logic_proc();
                redraw = true;
                break;
        	}
		}
		
       	/* draw after we have run all the logic */
		if(!(t3f_flags & T3F_USE_CONSOLE) && redraw && al_event_queue_is_empty(t3f_queue))
		{
			al_copy_transform(&t3f_current_transform, &t3f_base_transform);
			al_use_transform(&t3f_current_transform); // <-- apply additional transformations to t3f_current_transform
			t3f_render_proc();
			al_flip_display();
			redraw = false;
		}
	}
}

const char * t3f_get_filename(ALLEGRO_PATH * path, const char * fn)
{
	ALLEGRO_PATH * temp_path = al_clone_path(path);
	if(!temp_path)
	{
		return NULL;
	}
	al_set_path_filename(temp_path, fn);
	strcpy(t3f_return_filename, al_path_cstr(temp_path, '/'));
	al_destroy_path(temp_path);
	return t3f_return_filename;
}
