#ifndef OCD_ANIMATION_H
#define OCD_ANIMATION_H

#include <allegro5/allegro5.h>

#define T3F_ANIMATION_MAX_BITMAPS  256
#define T3F_ANIMATION_MAX_FRAMES  1024

#define T3F_ANIMATION_FLAG_ONCE      1

typedef struct
{
	
	int bitmap;
	float x; 
	float y;
	float z;
	float width;
	float height;
	float angle;
	int ticks;
	int flags;
	
} T3F_ANIMATION_FRAME;

typedef struct
{
	
	ALLEGRO_BITMAP * bitmap[T3F_ANIMATION_MAX_BITMAPS];
	int bitmaps;
	
	T3F_ANIMATION_FRAME * frame[T3F_ANIMATION_MAX_FRAMES];
	int frames;
	
	int frame_list[T3F_ANIMATION_MAX_FRAMES];
	int frame_list_total;
	
	int flags;
	
} T3F_ANIMATION;

/* memory management */
T3F_ANIMATION * t3f_create_animation(void);
T3F_ANIMATION * t3f_clone_animation(T3F_ANIMATION * ap);
void t3f_destroy_animation(T3F_ANIMATION * ap);
T3F_ANIMATION * t3f_load_animation_entry(ALLEGRO_FILE * fp);
T3F_ANIMATION * t3f_load_animation(const char * fn);
T3F_ANIMATION * t3f_load_animation_from_bitmap(const char * fn);
int t3f_save_animation_entry(T3F_ANIMATION * ap, ALLEGRO_FILE * fp);
int t3f_save_animation(T3F_ANIMATION * ap, const char * fn);

/* utilities */
int t3f_animation_add_bitmap(T3F_ANIMATION * ap, ALLEGRO_BITMAP * bp);
int t3f_animation_delete_bitmap(T3F_ANIMATION * ap, int bitmap);
int t3f_animation_add_frame(T3F_ANIMATION * ap, int bitmap, float x, float y, float z, float w, float h, float angle, int ticks);
int t3f_animation_delete_frame(T3F_ANIMATION * ap, int frame);
int t3f_animation_build_frame_list(T3F_ANIMATION * ap);
void t3f_add_animation_to_sprite_sheet(T3F_ANIMATION * ap);

/* in-game */
ALLEGRO_BITMAP * t3f_animation_get_bitmap(T3F_ANIMATION * ap, int tick);
T3F_ANIMATION_FRAME * t3f_animation_get_frame(T3F_ANIMATION * ap, int tick);
void t3f_draw_animation(T3F_ANIMATION * ap, ALLEGRO_COLOR color, int tick, float x, float y, float z, int flags);
void t3f_draw_scaled_animation(T3F_ANIMATION * ap, ALLEGRO_COLOR color, int tick, float x, float y, float z, float scale, int flags);
void t3f_draw_rotated_animation(T3F_ANIMATION * ap, ALLEGRO_COLOR color, int tick, float cx, float cy, float x, float y, float z, float angle, int flags);
void t3f_draw_rotated_scaled_animation(T3F_ANIMATION * ap, ALLEGRO_COLOR color, int tick, float cx, float cy, float x, float y, float z, float angle, float scale, int flags);

#endif
