void init_playlist_queues(void);
void queue_pending(sp_playlist *);
sp_playlist *dequeue_pending(void);
void queue_working(sp_playlist *);
void remove_working(sp_playlist *);
int still_working(void);
int deinit_finished_working(int(*grep)(sp_playlist *),void(*kill)(sp_playlist*));
void print_working(char *);
