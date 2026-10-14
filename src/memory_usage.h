#ifndef MEMORY_USAGE_H_INCLUDED__
#define MEMORY_USAGE_H_INCLUDED__

/* Readability sugar.
   Never use 'READABILITY_SUGAR_TEXTLOG_USAGE' directly,
   only use 'textlog_usage'. */
typedef struct {
	unsigned (*get) ();
	void (*add) (void *ptr, unsigned n);
	void (*remove) (void *ptr);
} READABILITY_SUGAR_TEXTLOG_USAGE;
extern const READABILITY_SUGAR_TEXTLOG_USAGE *textlog_usage;

/* Readability sugar.
   Never use 'READABILITY_SUGAR_IMAGELOG_USAGE' directly,
   only use 'imagelog_usage'. */
typedef struct {
	unsigned (*get) ();
	void (*add) (void *ptr, unsigned n);
	void (*remove) (void *ptr);
} READABILITY_SUGAR_IMAGELOG_USAGE;
extern const READABILITY_SUGAR_IMAGELOG_USAGE *imagelog_usage;

#endif //ndef MEMORY_USAGE_H_INCLUDED__
