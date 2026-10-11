#ifndef PIXEL_H_INCLUDED__
#define PIXEL_H_INCLUDED__

enum pixel_format {
	  PF_UNKNOWN = 0
	, PF_RGB1         // =                           (*)BITMAP bpp=1
	, PF_BGR888       // = RGB24(l.e.)  BGR24(b.e.)  (*)BITMAP bpp=24
	, PF_RGB888       // = BGR24(l.e.)  RGB24(b.e.)
	, PF_RGBA8888     // = ABGR32(l.e.) RGBA32(b.e.)

	, PF_END
};
uint8_t bpp_from_pixel_format (enum pixel_format pf);

#endif // PIXEL_H_INCLUDED__
