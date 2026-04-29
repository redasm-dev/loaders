#include "common.h"
#include "pe/constants.h"

bool mz_read_dos_header(RDReader* r, ImageDosHeader* dh) {
    rd_reader_read_le16(r, &dh->e_magic);

    if(dh->e_magic != IMAGE_DOS_SIGNATURE) return false;

    rd_reader_read_le16(r, &dh->e_cblp);
    rd_reader_read_le16(r, &dh->e_cp);
    rd_reader_read_le16(r, &dh->e_crlc);
    rd_reader_read_le16(r, &dh->e_cparhdr);
    rd_reader_read_le16(r, &dh->e_minalloc);
    rd_reader_read_le16(r, &dh->e_maxalloc);
    rd_reader_read_le16(r, &dh->e_ss);
    rd_reader_read_le16(r, &dh->e_sp);
    rd_reader_read_le16(r, &dh->e_csum);
    rd_reader_read_le16(r, &dh->e_ip);
    rd_reader_read_le16(r, &dh->e_cs);
    rd_reader_read_le16(r, &dh->e_lfarlc);
    rd_reader_read_le16(r, &dh->e_ovno);

    for(int i = 0; i < rd_count_of(dh->e_res); i++)
        rd_reader_read_le16(r, &dh->e_res[i]);

    rd_reader_read_le16(r, &dh->e_oemid);
    rd_reader_read_le16(r, &dh->e_oeminfo);

    for(int i = 0; i < rd_count_of(dh->e_res2); i++)
        rd_reader_read_le16(r, &dh->e_res2[i]);

    rd_reader_read_le32(r, &dh->e_lfanew);

    return !rd_reader_has_error(r);
}
