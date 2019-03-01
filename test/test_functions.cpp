#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include <chrono>

#include "test_functions.h"

using RBE = std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;

TestFunction::TestFunction()
{
    p_set_file_buf_size(0);
}
TestFunction::~TestFunction()
{
}

void TestFunction::CheckFatalErrors(pT_status status)
{
    if (status != P_OK) {
        fprintf(stderr, "Error no: %d, description: %s\n", (int)status, p_get_error_string(status));
        m_is_test_ok = false;
        throw status;
    } /* end of if (status != P_OK) */
    else {
        m_is_test_ok = true;
    }
}

std::string TestFunction::CreateStandardHeader(pT_header& header,
                                               pT_color color,
                                               pT_freq image_freq,
                                               pT_size image_size,
                                               int frm_nums)
{
    CheckFatalErrors(p_create_ext_header(&header, color, image_freq, image_size, 0, 1, P_4_3));
    CheckFatalErrors(p_mod_num_frames(&header, frm_nums));
    std::string fname = std::to_string(color) + "_" + std::to_string(image_freq) + "_" + std::to_string(image_size) + "_" + std::to_string(frm_nums) + ".pfspd";
    CheckFatalErrors(p_write_header(fname.c_str(), &header));

    return fname;
}

void TestFunction::FileWrite()
{
    try {
        auto tStart = std::chrono::high_resolution_clock::now();
        pT_header header;
        int y_w, y_h, uv_w, uv_h;
        int frm_nums      = 500;
        std::string fname = CreateStandardHeader(header, P_COLOR_420, P_60HZ, P_HDp, frm_nums);
        p_get_s_buffer_size(&header, &y_w, &y_h);
        p_get_uv_buffer_size(&header, &uv_w, &uv_h);
        std::cout << "num-comps:" << p_get_num_comps(&header) << " comps-size:y_w: " << y_w << " y_h:" << y_h << " uv_w:" << uv_w << " uv_h:" << uv_h << std::endl;
        TestDataInfo_t test_data_info;
        test_data_info.file_name = fname;
        RBE rbe;
        for (int32_t i = 1; i <= frm_nums; i++) {
            std::vector<unsigned char> data_y(y_w * y_h);
            std::vector<unsigned char> data_uv(uv_w * uv_h);
            std::generate(begin(data_y), end(data_y), std::ref(rbe));
            std::generate(begin(data_uv), end(data_uv), std::ref(rbe));
            CheckFatalErrors(
                p_write_frame(fname.c_str(),
                              &header,
                              i, /* frame nr    */
                              data_y.data(), /* Y buffer    */
                              data_uv.data(), /* U/V buffer  */
                              y_w, /* width       */
                              y_h, /* height      */
                              y_w) /* stride      */
            );

            FrmCrc32 frm_crc32;
            frm_crc32.comp_0 = crc32c::Crc32c(data_y.data(), data_y.size());
            frm_crc32.comp_1 = crc32c::Crc32c(data_uv.data(), data_uv.size());
            test_data_info.frm_crc32.push_back(frm_crc32);
        }

        m_test_files.push_back(test_data_info);
        auto tEnd      = std::chrono::high_resolution_clock::now();
        auto tDiff     = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
        int64_t offset = ((int64_t)(header.offset_hi) << 32) + ((int64_t)(header.offset_lo) & 0xFFFFFFFF);

        float speed = (double)(offset / (1024.0 * 1024.0)) / (tDiff / 1000000);
        std::cout << "Speed: " << speed << " M/S" << std::endl;
    } catch (pT_status e) {
        m_is_test_ok = false;
    }
}
void TestFunction::FileRead()
{
    try {
        for (const auto& test_file : m_test_files) {
            pT_header header;
            CheckFatalErrors(p_open_file(test_file.file_name.c_str(), 0));
            CheckFatalErrors(p_read_header(test_file.file_name.c_str(), &header));
            int width, height;
            int y_w, y_h, uv_w, uv_h;
            p_get_s_buffer_size(&header, &y_w, &y_h);
            p_get_uv_buffer_size(&header, &uv_w, &uv_h);
            width  = p_get_frame_width(&header);
            height = p_get_frame_height(&header);
            pT_color color_format = p_get_color_format (&header);
            std::cout<<"color_format:"<<color_format<<std::endl;
            for (int32_t frm = 1; frm <= header.nr_images; frm++) {

                std::vector<unsigned char> data_y(y_w*y_h);
                std::vector<unsigned char> data_uv(uv_w*uv_h);
                CheckFatalErrors(p_read_frame(test_file.file_name.c_str(), &header, frm, data_y.data(), data_uv.data(), P_READ_ALL, width, height, width));

                FrmCrc32 frm_crc32;
                frm_crc32.comp_0 = crc32c::Crc32c(data_y.data(), data_y.size());
                frm_crc32.comp_1 = crc32c::Crc32c(data_uv.data(), data_uv.size());

                if ((test_file.frm_crc32[frm-1].comp_0 != frm_crc32.comp_0) || (test_file.frm_crc32[frm-1].comp_1 != frm_crc32.comp_1)) {
                    std::cout<<"CRC32 not matched:"<<frm<<std::endl;
                    throw P_READ_FAILED;
                }
            }
        }
    } catch (pT_status e) {
        m_is_test_ok = false;
    }
}