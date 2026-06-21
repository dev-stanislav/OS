#include "users.h"
#include "fs.h"
#include "libk.h"
#include "vga.h"

#define USER_MAX 8
#define USER_NAME_MAX 16

typedef struct { char name[USER_NAME_MAX + 1]; uint8_t root; } user_t;
static user_t users[USER_MAX];
static uint8_t user_count, current_user;

static uint8_t valid_name(const char *name) { uint8_t length=0;while(*name){char c=*name++;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;if(++length>USER_NAME_MAX)return 0;}return length>0; }
static int find_user(const char *name) { for(uint8_t i=0;i<user_count;i++)if(kstrcmp(users[i].name,name)==0)return i;return -1; }
static void ensure_home(const char *name) { char path[40]="/home/";(void)fs_create("/home",FS_DIR,fs_root());kstrcpy(path+6,name,sizeof(path)-6);(void)fs_create(path,FS_DIR,fs_root()); }
static void save_users(void) { char text[USER_MAX*24+1];uint16_t out=0;for(uint8_t i=0;i<user_count;i++){for(uint8_t j=0;users[i].name[j];j++)text[out++]=users[i].name[j];text[out++]=':';const char *role=users[i].root?"root":"user";for(uint8_t j=0;role[j];j++)text[out++]=role[j];text[out++]='\n';}text[out]='\0';(void)fs_write("/system/users.db",text,fs_root()); }
static void add_loaded(const char *name,uint8_t root) { if(user_count>=USER_MAX||!valid_name(name)||find_user(name)>=0)return;kstrcpy(users[user_count].name,name,sizeof(users[user_count].name));users[user_count++].root=root;ensure_home(name); }
static void load_users(void) { int index=fs_resolve("/system/users.db",fs_root());const fs_node_t*n=fs_node(index);if(!n)return;char name[USER_NAME_MAX+1];uint16_t pos=0;for(uint16_t i=0;i<=n->size;i++){char c=n->data[i];if(c==':'||c=='\n'||c=='\0'){if(c==':'){name[pos]='\0';uint8_t root=(i+1<n->size&&n->data[i+1]=='r');while(i<n->size&&n->data[i]!='\n')i++;add_loaded(name,root);pos=0;}else pos=0;}else if(pos<USER_NAME_MAX)name[pos++]=c;} }

void users_init(void) { user_count=0;current_user=0;(void)fs_create("/system",FS_DIR,fs_root());load_users();if(!user_count){add_loaded("root",1);save_users();}int root=find_user("root");current_user=root>=0?(uint8_t)root:0;fs_set_actor(current_user,users[current_user].root); }
uint8_t users_is_root(void) { return users[current_user].root; }
const char *users_current_name(void) { return users[current_user].name; }
const char *users_current_role(void) { return users[current_user].root?"root":"user"; }
const char *users_current_home(void) { static char path[40]="/home/";kstrcpy(path+6,users[current_user].name,sizeof(path)-6);return path; }
uint8_t users_current_uid(void){return current_user;}
int users_find_uid(const char *name){return find_user(name);}
void users_list(void) { vga_write("NAME             ROLE\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);for(uint8_t i=0;i<user_count;i++){vga_write(users[i].name,i==current_user?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);for(uint8_t j=(uint8_t)kstrlen(users[i].name);j<17;j++)vga_putchar(' ',VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(users[i].root?"root\n":"user\n",users[i].root?VGA_COLOR_LIGHT_BROWN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);} }
void users_add(const char *name,const char *role) { if(!users_is_root()){vga_write("only root can add users\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}if(user_count>=USER_MAX){vga_write("user limit reached\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}if(!valid_name(name)||find_user(name)>=0){vga_write("invalid or existing user name\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}if(kstrcmp(role,"root")&&kstrcmp(role,"user")){vga_write("role must be root or user\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}add_loaded(name,kstrcmp(role,"root")==0);save_users();vga_write("user created\n",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK); }
uint8_t users_login(const char *name) { int index=find_user(name);if(index<0){vga_write("user not found\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return 0;}current_user=(uint8_t)index;fs_set_actor(current_user,users[current_user].root);vga_write("logged in as ",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write(users[current_user].name,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);return 1; }
