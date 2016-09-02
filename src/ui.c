#include "obbg_funcs.h"
#include "SDL.h"
#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"

#pragma warning(disable:4305; disable:4244)
enum
{
   UI_SCREEN_none,
   UI_SCREEN_select
};
int ui_screen=UI_SCREEN_none;

void obbg_drawRect(float x0, float y0, float x1, float y1)
{
   glTexCoord2f(0,0); glVertex2f(x0,y0);
   glTexCoord2f(1,0); glVertex2f(x1,y0);
   glTexCoord2f(1,1); glVertex2f(x1,y1);
   glTexCoord2f(0,1); glVertex2f(x0,y1);
}

void obbg_drawRectTC(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1)
{
   glTexCoord2f(s0,t0); glVertex2f(x0,y0);
   glTexCoord2f(s1,t0); glVertex2f(x1,y0);
   glTexCoord2f(s1,t1); glVertex2f(x1,y1);
   glTexCoord2f(s0,t1); glVertex2f(x0,y1);
}

int selected_block[3];
int selected_block_to_create[3];
Bool selected_block_valid;
static int block_rotation;
static int block_choice = 0;
static int mouse_x, mouse_y;

int actionbar_blocktype[9] =
{
   BT_conveyor,
   // BT_conveyor_90_left, BT_conveyor_90_right,
   //BT_conveyor_ramp_up_low, BT_conveyor_ramp_down_high, BT_splitter,
   //BT_stone, BT_asphalt,
   //BT_picker, BT_ore_drill,
};


void rotate_block(void)
{
   int block = get_block(selected_block[0], selected_block[1], selected_block[2]);
   if (block >= BT_placeable) {
      int rot = get_block_rot(selected_block[0], selected_block[1], selected_block[2]);
      rot = (rot+1) & 3;
      block_rotation = rot;
      change_block(selected_block[0], selected_block[1], selected_block[2], block, rot);
   }
}

static int quit;
int hack_ffwd;

void active_control_set(int key)
{
   client_player_input.buttons |= 1 << key;
}

void active_control_clear(int key)
{
   client_player_input.buttons &= ~(1 << key);
}

typedef struct
{
   int y_base, y_size;
   int x_base, x_size, x_spacing;
   int count;
} ui_rect_row;

int sprite_for_blocktype[256];

void get_coordinates(ui_rect_row *row, int item, int *x, int *y)
{
   *x = row->x_base + (row->x_size + row->x_spacing)*item;
   *y = row->y_base;
}

int hit_detect_row(ui_rect_row *row, recti *box)
{
   int i;
   int best_i=-1;
   float best_overlap=0;
   int x_advance = row->x_size + row->x_spacing;
   float x0 = row->x_base;
   float y0 = row->y_base;
   float y1 = y0 + row->y_size;

   static recti mbox = { 0,0,0,0 };
   if (box == NULL)
      box = &mbox;

   //     ax0................ax1
   //             bx0................bx1
   //              ^          ^
   //        max(ax0,bx0)     |
   //                       min(ax1,bx1)

   for (i=0; i < row->count; ++i) {
      float x1 = x0 + row->x_size;
      if (mouse_x+box->x0 < x1 && mouse_x+box->x1 > x0 &&
          mouse_y+box->y0 < y1 && mouse_y+box->y1 > y0) {
         if (box == &mbox)
            return i;
         else {
            float x_min = stb_max(mouse_x+box->x0, x0);
            float y_min = stb_max(mouse_y+box->y0, y0);
            float x_max = stb_min(mouse_x+box->x1, x1);
            float y_max = stb_min(mouse_y+box->y1, y1);
            float overlap = (x_max - x_min) * (y_max - y_min);
            assert(x_min <= x_max && y_min <= y_max);
            assert(overlap >= 0);
            if (overlap > best_overlap) {
               best_overlap = overlap;
               best_i = i;
            }
         }
      }
      x0 += x_advance;
   }
   return best_i;
}

void draw_ui_row(ui_rect_row *row, int *blockcodes, int hit_item, int choice)
{
   int i;
   int x_advance = row->x_size + row->x_spacing;
   float x0,y0,x1,y1;


   y0 = row->y_base;
   y1 = y0 + row->y_size;
   x0 = row->x_base;
   x1 = x0 + row->x_size;
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_BLEND);
   glBegin(GL_QUADS);
   for (i=0; i < row->count; ++i) {
      if (choice == i) {
         glColor3f(0.8,0.4,0.8);
         obbg_drawRect(x0-5,y0-5,x1+5,y1+5);
      }
      if (hit_item==i) {
         glColor3f(1.0,1.0,1.0);
         obbg_drawRect(x0-3,y0-3,x1+3,y1+3);
      }
      glColor3f(0.9,0.9,0.9);
      obbg_drawRect(x0,y0,x1,y1);

      x0 += x_advance;
      x1 += x_advance;
   }
   glEnd();

   y0 = row->y_base;
   y1 = y0 + row->y_size;
   x0 = row->x_base;
   x1 = x0 + row->x_size;
   glEnable(GL_TEXTURE_2D);
   glEnable(GL_BLEND);
   //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // it's really premultiplied alpha so should be this one
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // but hackily this produces outlines that makes it more readable
   for (i=0; i < row->count; ++i) {
      if (blockcodes != NULL) {
         int sprite = sprite_for_blocktype[blockcodes[i]];
         if (sprite != 0) {
            glBindTexture(GL_TEXTURE_2D, sprite);
            glBegin(GL_QUADS);
            obbg_drawRectTC(x0,y0,x1,y1, 0,0,1,1);
            glEnd();
         }
      }

      x0 += x_advance;
      x1 += x_advance;
   }
}

ui_rect_row ui_actionbar[1];
ui_rect_row ui_inventory[3];

int inventory_blocktype[3][9] =
{
   { BT_stone, BT_asphalt, } ,
   { BT_picker, BT_ore_drill, BT_furnace, BT_iron_gear_maker, BT_conveyor_belt_maker },
   { BT_conveyor, BT_conveyor_90_left, BT_conveyor_90_right,
     BT_conveyor_ramp_up_low, BT_conveyor_ramp_down_high, BT_splitter, },
};

void draw_action_bar(int hit_item)
{
   draw_ui_row(ui_actionbar, actionbar_blocktype, hit_item, block_choice);
}

float left_x_to_center_contents_of_width(float size)
{
   return screen_x/2 - size/2;
}

float top_y_to_center_contents_of_height(float size)
{
   return screen_y/2 - size/2;
}

void compute_ui_actionbar(void)
{
   ui_rect_row *rr = &ui_actionbar[0];
   rr->x_size = 70;
   rr->y_size = 70;
   rr->x_spacing = 12;
   rr->count = 9;
   rr->y_base = screen_y - rr->y_size;
   rr->x_base = left_x_to_center_contents_of_width(rr->x_size * rr->count + rr->x_spacing*(rr->count-1));
}

void compute_ui_inventory(void)
{
   int i;
   ui_rect_row *rr;
   int y_spacing = 8;
   int y_size = 90;
   int advance = y_size + y_spacing;

   for (i=0; i < 3; ++i) {
      ui_inventory[i].x_size = 90;
      ui_inventory[i].y_size = y_size;
      ui_inventory[i].x_spacing = 8;
      ui_inventory[i].count = 9;
   }

   rr = &ui_inventory[0];
   ui_inventory[1].x_base =
   ui_inventory[2].x_base = 
   rr->x_base = left_x_to_center_contents_of_width(rr->x_size * rr->count + rr->x_spacing*(rr->count-1));

   rr->y_base = top_y_to_center_contents_of_height(rr->y_size * 3 + y_spacing*2);
   ui_inventory[1].y_base = ui_inventory[0].y_base + advance;
   ui_inventory[2].y_base = ui_inventory[1].y_base + advance;
}

static Bool dragging = False;
static int mouse_drag_offset_x, mouse_drag_offset_y;
static int drag_item;

int blocktype_mouse_is_over(void)
{
   int j;
   for (j=0; j < 3; ++j) {
      int hit = hit_detect_row(&ui_inventory[j], NULL);
      if (hit >= 0) {
         return inventory_blocktype[j][hit];
      }
   }
   return -1;
}

void mouse_down(int button)
{
   dragging = False;
   switch (ui_screen) {
      case UI_SCREEN_select: {
         int j;
         for (j=0; j < 3; ++j) {
            int hit = hit_detect_row(&ui_inventory[j], NULL);
            if (hit >= 0) {
               int x,y;
               dragging = True;
               get_coordinates(&ui_inventory[j], hit, &x,&y);
               mouse_drag_offset_x = x - mouse_x;
               mouse_drag_offset_y = y - mouse_y;
               drag_item = inventory_blocktype[j][hit];
            }
         }
         break;
      }
      
      case UI_SCREEN_none:
         if (selected_block_valid) {
            if (button == 1)
               change_block(selected_block[0], selected_block[1], selected_block[2], BT_empty, block_rotation);
            else if (button == -1) {
               int block = actionbar_blocktype[block_choice];
               if (block != BT_empty)
                  change_block(selected_block_to_create[0], selected_block_to_create[1], selected_block_to_create[2], block, block_rotation);
            }
         }
         break;
   }
}


void mouse_up(void)
{
   if (dragging) {
      int hit;
      recti shape;
      shape.x0 = mouse_drag_offset_x;
      shape.y0 = mouse_drag_offset_y;
      shape.x1 = shape.x0 + ui_inventory[0].x_size;
      shape.y1 = shape.y0 + ui_inventory[0].y_size;
      hit = hit_detect_row(ui_actionbar, &shape);
      if (hit >= 0) {
         actionbar_blocktype[hit] = drag_item;
      }
      dragging = False;
   }
}

extern void update_view(float dx, float dy);

static Bool first_mouse=True;

void  process_mouse_move(int dx, int dy)
{
   if (ui_screen != UI_SCREEN_none) {
      mouse_x += dx*2;
      mouse_y += dy*2;
      mouse_x = stb_clamp(mouse_x, 0, screen_x);
      mouse_y = stb_clamp(mouse_y, 0, screen_y);
   } else {
      update_view((float) dx / screen_x, (float) dy / screen_y);
   }
}

void process_key_down(int k, int s, SDL_Keymod mod)
{
   if (k == SDLK_ESCAPE)
      quit = 1;

   // player movement
   if (s == SDL_SCANCODE_D)    active_control_set(0);
   if (s == SDL_SCANCODE_A)    active_control_set(1);
   if (s == SDL_SCANCODE_W)    active_control_set(2);
   if (s == SDL_SCANCODE_S)    active_control_set(3);
   if (k == SDLK_SPACE)        active_control_set(4); 
   if (s == SDL_SCANCODE_LCTRL)active_control_set(5);
   if (s == SDL_SCANCODE_S)    active_control_set(6);
   if (s == SDL_SCANCODE_D)    active_control_set(7);
   if (s == SDL_SCANCODE_F)    client_player_input.flying = !client_player_input.flying;

   // debugging
   if (s == SDL_SCANCODE_H) global_hack = !global_hack;
   if (s == SDL_SCANCODE_P) debug_render = !debug_render;
   if (s == SDL_SCANCODE_C) show_memory = !show_memory;//examine_outstanding_genchunks();

   if (s == SDL_SCANCODE_E) {
      if (ui_screen != UI_SCREEN_none) {
         ui_screen = UI_SCREEN_none;
      } else {
         ui_screen = UI_SCREEN_select;
         compute_ui_inventory();
         first_mouse = True;
      }
   }

   if (ui_screen != UI_SCREEN_none && first_mouse) {
      first_mouse = False;
      mouse_x = screen_x/2;
      mouse_y = screen_y/2;
   }
   if (s == SDL_SCANCODE_R)    rotate_block();
   if (s == SDL_SCANCODE_M)    save_edits();
   if (s == SDL_SCANCODE_TAB)    hack_ffwd = !hack_ffwd;

   if (k >= '1' && k <= '9') {
      block_choice = k-'1';
      if (ui_screen == UI_SCREEN_select) {
         int selected_block_type = blocktype_mouse_is_over();
         if (selected_block_type >= 0) {
            actionbar_blocktype[block_choice] = selected_block_type;
         }
      }
   }
   //if (k == '6') block_base = BT_conveyor_up_east_low;
   //if (k == '2') global_hack = -1;
   //if (k == '3') obj[player_id].position.x += 65536;
   #if 0
   if (s == SDL_SCANCODE_R) {
      objspace_to_worldspace(light_vel, player_id, 0,32,0);
      memcpy(light_pos, &obj[player_id].position, sizeof(light_pos));
   }
   #endif

   #if 0
   if (game_mode == GAME_editor) {
      switch (k) {
         case SDLK_RIGHT: editor_key(STBTE_scroll_right); break;
         case SDLK_LEFT : editor_key(STBTE_scroll_left ); break;
         case SDLK_UP   : editor_key(STBTE_scroll_up   ); break;
         case SDLK_DOWN : editor_key(STBTE_scroll_down ); break;
      }
      switch (s) {
         case SDL_SCANCODE_S: editor_key(STBTE_tool_select); break;
         case SDL_SCANCODE_B: editor_key(STBTE_tool_brush ); break;
         case SDL_SCANCODE_E: editor_key(STBTE_tool_erase ); break;
         case SDL_SCANCODE_R: editor_key(STBTE_tool_rectangle ); break;
         case SDL_SCANCODE_I: editor_key(STBTE_tool_eyedropper); break;
         case SDL_SCANCODE_L: editor_key(STBTE_tool_link); break;
         case SDL_SCANCODE_G: editor_key(STBTE_act_toggle_grid); break;
      }
      if ((e->key.keysym.mod & KMOD_CTRL) && !(e->key.keysym.mod & ~KMOD_CTRL)) {
         switch (s) {
            case SDL_SCANCODE_X: editor_key(STBTE_act_cut  ); break;
            case SDL_SCANCODE_C: editor_key(STBTE_act_copy ); break;
            case SDL_SCANCODE_V: editor_key(STBTE_act_paste); break;
            case SDL_SCANCODE_Z: editor_key(STBTE_act_undo ); break;
            case SDL_SCANCODE_Y: editor_key(STBTE_act_redo ); break;
         }
      }
   }
   #endif
}

void process_key_up(int k, int s)
{
   if (s == SDL_SCANCODE_D)   active_control_clear(0);
   if (s == SDL_SCANCODE_A)   active_control_clear(1);
   if (s == SDL_SCANCODE_W)   active_control_clear(2);
   if (s == SDL_SCANCODE_S)   active_control_clear(3);
   if (k == SDLK_SPACE)       active_control_clear(4); 
   if (s == SDL_SCANCODE_LCTRL)   active_control_clear(5);
   if (s == SDL_SCANCODE_S)   active_control_clear(6);
   if (s == SDL_SCANCODE_D)   active_control_clear(7);
}


GLuint icon_arrow;
void do_ui_rendering_2d(void)
{
   compute_ui_actionbar();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);

   {
      float cx = screen_x / 2.0f;
      float cy = screen_y / 2.0f;
      glColor3f(1,1,1);
      glBegin(GL_LINES);
      glVertex2f(cx-4,cy); glVertex2f(cx+4,cy);
      glVertex2f(cx,cy-3); glVertex2f(cx,cy+3);
      glEnd();
   }

   switch (ui_screen) {
      case UI_SCREEN_select: {
         int hit;
         int j;
         glColor3f(0.7,0.7,0.7);
         //stbgl_drawRect(screen_x*1.0/4.0, screen_y*1.0/4.0, screen_x*3.0/4.0, screen_y*3.0/4.0);
         for (j=0; j < 3; ++j) {
            int ihit = dragging ? -1 : hit_detect_row(&ui_inventory[j], NULL);
            draw_ui_row(&ui_inventory[j], &inventory_blocktype[j][0], ihit, -1);
         }
         hit = -1;
         if (dragging) {
            recti shape;
            shape.x0 = mouse_drag_offset_x;
            shape.y0 = mouse_drag_offset_y;
            shape.x1 = shape.x0 + ui_inventory[0].x_size;
            shape.y1 = shape.y0 + ui_inventory[0].y_size;
            hit = hit_detect_row(&ui_actionbar[0], &shape);
         }
         draw_action_bar(hit);
         break;
      }
      case UI_SCREEN_none: {
         draw_action_bar(-1);
         break;
      }
   }

   if (ui_screen != UI_SCREEN_none) {
      float mx,my;

      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      if (dragging) {
         mx = mouse_x + mouse_drag_offset_x;
         my = mouse_y + mouse_drag_offset_y;

         glDisable(GL_TEXTURE_2D);
         glColor4f(0.3,0.3,0.3,0.3);
         stbgl_drawRect(mx,my, mx+ui_inventory[0].x_size,my+ui_inventory[0].y_size);

         glEnable(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, sprite_for_blocktype[drag_item]);
         glColor3f(1,1,1);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // hack to get outline
         stbgl_drawRectTC(mx,my, mx+ui_inventory[0].x_size,my+ui_inventory[0].y_size, 0,0,1,1);
      }

      mx = mouse_x - 5;
      my = mouse_y - 6;

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, icon_arrow);
      stbgl_drawRectTC(mx,my, mx+32,my+32, 0,0,1,1);
      glDisable(GL_BLEND);
      glDisable(GL_TEXTURE_2D);
   }
}

void do_ui_rendering_3d(void)
{
   int i;
   vec pos[2];
   RaycastResult result;
   // show wireframe of currently 'selected' block
   objspace_to_worldspace(&pos[1].x, player_id, 0,9,0);
   pos[0] = obj[player_id].position;
   pos[1].x += pos[0].x;
   pos[1].y += pos[0].y;
   pos[1].z += pos[0].z;
   selected_block_valid = raycast(pos[0].x, pos[0].y, pos[0].z, pos[1].x, pos[1].y, pos[1].z, &result);
   if (selected_block_valid) {
      for (i=0; i < 3; ++i) {
         selected_block[i] = (&result.bx)[i];
         selected_block_to_create[i] = (&result.bx)[i] + face_dir[result.face][i];
      }
      glColor3f(0.7f,1.0f,0.7f);
      //stbgl_drawBox(selected_block_to_create[0]+0.5f, selected_block_to_create[1]+0.5f, selected_block_to_create[2]+0.5f, 1.2f, 1.2f, 1.2f, 0);
      stbgl_drawBox(selected_block[0]+0.5f, selected_block[1]+0.5f, selected_block[2]+0.5f, 1.2f, 1.2f, 1.2f, 0);
   }
}

struct
{
   int blocktype;
   char *sprite_name;
} sprite_filenames[] =
{
   { BT_conveyor               , "conveyor"         },
   { BT_conveyor_90_left       , "conveyor_90_ccw"  },
   { BT_conveyor_90_right      , "conveyor_90_cw"   },
   { BT_conveyor_ramp_up_low   , "conveyor_up"      },
   { BT_conveyor_ramp_down_high, "conveyor_down"    },
   { BT_splitter               , "splitter"         },
   { BT_furnace                , "furnace"          },
   { BT_stone                  , "iron_block"       },
   { BT_asphalt                , "coal"             },
   { BT_iron_gear_maker        , "iron_gear_maker"  },
   { BT_conveyor_belt_maker    , "conveyor_belt_maker" },
   { BT_picker                 , "picker"           },
   { BT_ore_drill              , "drill"            },
};

void init_ui_render(void)
{
   int i;
   icon_arrow = load_sprite("data/sprites/icon_arrow.png");

   for (i=0; i < sizeof(sprite_filenames)/sizeof(sprite_filenames[0]); ++i)
      sprite_for_blocktype[sprite_filenames[i].blocktype] = load_sprite(stb_sprintf("data/sprites/icon_%s.png", sprite_filenames[i].sprite_name));

}
