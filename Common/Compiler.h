/**
 * @file Compiler.h
 * @brief Macro mô tả khai báo C. Với trình biên dịch hiện tại, FUNC giữ kiểu trả về, P2CONST tạo con trỏ tới dữ liệu chỉ đọc và các tham số lớp bộ nhớ được bỏ qua.
 */
#ifndef COMPILER_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define COMPILER_H

/*==============================================================================
 * COMPILER ABSTRACTION MACROS (SWS_CompilerAbstraction R4.4.0)
 *============================================================================*/
/** @brief Đánh dấu lớp lưu trữ tự động, mở rộng rỗng trên trình biên dịch này. */
#define AUTOMATIC
/** @brief Đánh dấu lớp dùng khi khai báo kiểu, mở rộng rỗng ở đây. */
#define TYPEDEF

#ifndef NULL_PTR
    /** @brief Con trỏ rỗng để kiểm tra tham số địa chỉ. */
    #define NULL_PTR        ((void *)0)
#endif

/** @brief Gợi ý trình biên dịch chèn thân hàm tại nơi gọi. */
#define INLINE              inline
/** @brief Hàm inline có liên kết nội bộ. */
#define LOCAL_INLINE        static inline

/** @brief Khai báo hàm trả về rettype. */
/* Trên trình biên dịch này, memclass và ptrclass không tham gia chọn vùng nhớ. Dấu const trước kiểu khóa dữ liệu, sau dấu * khóa con trỏ. */
#define FUNC(rettype, memclass)                         rettype
/** @brief Con trỏ tới dữ liệu có thể sửa. */
#define P2VAR(ptrtype, memclass, ptrclass)              ptrtype *
/** @brief Con trỏ tới dữ liệu chỉ đọc qua con trỏ. */
#define P2CONST(ptrtype, memclass, ptrclass)            const ptrtype *
/** @brief Con trỏ cố định tới dữ liệu có thể sửa. */
#define CONSTP2VAR(ptrtype, memclass, ptrclass)         ptrtype * const
/** @brief Con trỏ cố định tới dữ liệu chỉ đọc qua con trỏ. */
#define CONSTP2CONST(ptrtype, memclass, ptrclass)       const ptrtype * const
/** @brief Khai báo đối tượng chỉ đọc. */
#define CONST(consttype, memclass)                      const consttype
/** @brief Khai báo biến có kiểu vartype. */
#define VAR(vartype, memclass)                          vartype

#endif /* COMPILER_H */
