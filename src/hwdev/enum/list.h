// TODO: generate this dynamically

#ifndef _HWDEV_ENUM_LIST_H_
#define _HWDEV_ENUM_LIST_H_ 1

#ifdef _WIN32
extern const hw_enum_t hw_enum_bda;
#endif

#ifdef HAVE_DVBAPI
extern const hw_enum_t hw_enum_dvbapi;
#endif

static const hw_enum_t *enum_list[] =
{
#ifdef _WIN32
    &hw_enum_bda,
#endif
#ifdef HAVE_DVBAPI
    &hw_enum_dvbapi,
#endif
    NULL,
};

#endif /* _HWDEV_ENUM_LIST_H_ */
