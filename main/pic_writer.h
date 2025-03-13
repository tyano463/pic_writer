#ifndef __PIC_WRITER_H__
#define __PIC_WRITER_H__

#define ERR_RETn(c)            \
    {                          \
        if (c)                 \
            goto error_return; \
    }

#endif