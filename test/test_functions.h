#ifndef _TEST_FUNCTIONS_H
#define _TEST_FUNCTIONS_H

#include <vector>
#include <string>
#include "cpfspd.h"
#include "crc32c/crc32c.h"

typedef struct _FrmCrc32
{
    uint32_t comp_0 = 0;
    uint32_t comp_1 = 0;
    uint32_t comp_2 = 0;
}FrmCrc32;
typedef struct _TestDataInfo_t {
    std::string file_name;
    int64_t file_zie;
    pT_header pfspd_header;
    std::vector<FrmCrc32> frm_crc32;
} TestDataInfo_t;

class TestFunction
{
    public:
    TestFunction();
    ~TestFunction();
    void FileWrite();
    void FileRead();
    bool IsTeskOk(){return m_is_test_ok;}

    private:
    std::string  CreateStandardHeader(pT_header& header,
                      pT_color color,
                      pT_freq image_freq,
                      pT_size image_size,
                      int frm_nums);
    void CheckFatalErrors(pT_status status);

    private:
    std::vector<TestDataInfo_t> m_test_files;
    bool m_is_test_ok = false;
};
#endif //_TEST_FUNCTIONS_H