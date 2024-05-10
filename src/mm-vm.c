//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef MM_PAGING


/**
 * enlist_vm_freerg_list - Thêm một khu vực mới vào danh sách các khu vực trống.
 * @mm: Cấu trúc quản lý bộ nhớ.
 * @rg_elmt: Khu vực mới cần được thêm vào.
 *
 * Trả về: 0 nếu thành công, -1 nếu khu vực mới không hợp lệ.
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt){
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

    //Kiểm tra nếu đầu của khu vực mới lớn hơn hoặc bằng cuối của nó
    if (rg_elmt->rg_start >= rg_elmt->rg_end) 
        return -1;

    //Liên kết khu vực mới với danh sách các khu vực trống hiện tại
    if (rg_node != NULL) 
        rg_elmt->rg_next = rg_node;

    //Thêm khu vực mới vào đầu danh sách các khu vực trống
    mm->mmap->vm_freerg_list = rg_elmt;

    return 0;
}

/**
 * get_vma_by_num - Lấy khu vực bộ nhớ ảo theo ID số.
 * @mm: Cấu trúc quản lý bộ nhớ.
 * @vmaid: ID của khu vực bộ nhớ ảo để phân bổ khu vực bộ nhớ.
 *
 * Trả về: Con trỏ đến khu vực bộ nhớ ảo nếu tìm thấy, ngược lại trả về NULL.
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid){
    struct vm_area_struct *pvma = mm->mmap;

    // Kiểm tra nếu danh sách bản đồ bộ nhớ là NULL
    if (mm->mmap == NULL)
        return NULL;

    int vmait = 0;

    //Duyệt qua danh sách các khu vực bộ nhớ ảo cho đến khi tìm thấy ID phù hợp
    while (vmait < vmaid)
    {
        if (pvma == NULL)
            return NULL;

        vmait++;
        pvma = pvma->vm_next; //Chuyển đến khu vực bộ nhớ ảo tiếp theo
    }

    return pvma;
}

/**
 * get_symrg_byid - Lấy khu vực bộ nhớ dựa trên ID của khu vực đó.
 * @mm: Cấu trúc quản lý bộ nhớ.
 * @rgid: ID khu vực, hoạt động như chỉ số biểu tượng của biến.
 *
 * Trả về: Con trỏ đến khu vực bộ nhớ nếu ID hợp lệ, ngược lại trả về NULL.
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid){
    //Kiểm tra tính hợp lệ của ID khu vực
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;

    //Trả về con trỏ đến khu vực bộ nhớ trong bảng biểu tượng dựa trên ID
    return &mm->symrgtbl[rgid];
}

/**
 * __alloc - Phân bổ một khu vực bộ nhớ.
 * @caller: Đối tượng gọi hàm.
 * @vmaid: ID của khu vực bộ nhớ ảo để phân bổ khu vực bộ nhớ.
 * @rgid: ID khu vực bộ nhớ (dùng để xác định biến trong bảng ký hiệu).
 * @size: Kích thước cần phân bổ.
 * @alloc_addr: Địa chỉ của khu vực bộ nhớ được phân bổ.
 *
 * Trả về: 0 nếu thành công, giá trị khác nếu thất bại.
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr){
    //Phân bổ tại root
    struct vm_rg_struct rgnode;

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

        *alloc_addr = rgnode.rg_start;

        pthread_mutex_unlock(&mem_lock);
        return 0;
    }

    //Xử lý nếu get_free_vmrg_area không thành công
    //Cố tăng giới hạn để lấy thêm không gian
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);

    int old_sbrk = cur_vma->sbrk;

    //Tăng giới hạn
    inc_vma_limit(caller, vmaid, inc_sz);

    //Tăng giới hạn thành công
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

    *alloc_addr = old_sbrk;

    //Thêm khu vực còn lại vào danh sách các khu vực trống
    if (old_sbrk + size < cur_vma->vm_end)
    {
        struct vm_rg_struct *new_rgnode = malloc(sizeof(struct vm_rg_struct));
        new_rgnode->rg_start = old_sbrk + size;
        new_rgnode->rg_end = cur_vma->vm_end;
        enlist_vm_freerg_list(caller->mm, new_rgnode);
    }
    
    pthread_mutex_unlock(&mem_lock);
    return 0;
}

/**
 * __free - gỡ bỏ một vùng nhớ
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @rgid: ID vùng nhớ (dùng để nhận diện biến trong bảng ký hiệu)
 * @size: kích thước đã cấp phát (không được sử dụng trong hàm này)
 */
int __free(struct pcb_t *caller, int vmaid, int rgid) {
    pthread_mutex_lock(&mem_lock); //Khóa mutex

    struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct)); // Cấp phát bộ nhớ cho nút vùng nhớ

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
        pthread_mutex_unlock(&mem_lock); //Mở khóa mutex nếu ID không hợp lệ
        return -1; //Trả về lỗi nếu ID vùng nhớ không hợp lệ
    }

    // TODO: Quản lý việc thu gom vùng nhớ đã giải phóng vào danh sách freerg_list
    rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start; //Đặt điểm bắt đầu
    rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end; //Đặt điểm kết thúc

    //Thêm vùng nhớ đã không còn sử dụng vào danh sách
    enlist_vm_freerg_list(caller->mm, rgnode);

    pthread_mutex_unlock(&mem_lock); //Mở khóa mutex
    return 0; //Trả về 0 nếu thành công
}


/**
 * pgalloc - Phân bổ bộ nhớ dựa trên phân trang cho một khu vực bộ nhớ.
 * @proc: Quy trình thực hiện lệnh.
 * @size: Kích thước cần phân bổ.
 * @reg_index: ID khu vực bộ nhớ (dùng để xác định biến trong bảng ký hiệu).
 *
 * Trả về: Địa chỉ của khu vực bộ nhớ được phân bổ nếu thành công.
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    int addr;

    //Mặc định sử dụng vmaid = 0
    return __alloc(proc, 0, reg_index, size, &addr);
}

/**
 * pgfree_data - Giải phóng bộ nhớ dựa trên phân trang cho một khu vực bộ nhớ.
 * @proc: Quy trình thực hiện lệnh.
 * @reg_index: ID khu vực bộ nhớ (dùng để xác định biến trong bảng ký hiệu).
 *
 * Trả về: 0 nếu giải phóng thành công, giá trị khác nếu thất bại.
 */
int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
    return __free(proc, 0, reg_index);
}

/**
 * pg_getpage - lấy trang trong RAM
 * @mm: vùng nhớ
 * @pagenum: số trang PGN
 * @framenum: trả về số frame FPN
 * @caller: người gọi
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {
    uint32_t pte = mm->pgd[pgn];
    if (!PAGING_PAGE_PRESENT(pte)) { //Khung chưa được gán cho trang này
        int new_fpn;
        if (MEMPHY_get_freefp(caller->mram, &new_fpn) < 0) { //Không có khung trống trong RAM
            int vicpgn;
            find_victim_page(caller->mm, &vicpgn);
            uint32_t vic_pte = mm->pgd[vicpgn];
            int vicfpn = PAGING_FPN(vic_pte);
            int swpfpn;
            MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
            __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
            pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);
            new_fpn = vicfpn;
        }
        pte_set_fpn(&mm->pgd[pgn], new_fpn);
    } else if (pte & PAGING_PTE_SWAPPED_MASK) { //Trang không trực tuyến, làm cho nó hoạt động trực tiếp
        int vicpgn, swpfpn;
        int tgtfpn = PAGING_SWP(pte);

        //TODO: Thực hiện với lý thuyết phân trang ở đây
        //Tìm trang nạn nhân
        find_victim_page(caller->mm, &vicpgn);
        uint32_t vic_pte = mm->pgd[vicpgn];
        int vicfpn = PAGING_FPN(vic_pte);

        //Lấy khung trống trong MEMSWP
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

        //Thực hiện hoán đổi khung từ MEMRAM sang MEMSWP và ngược lại
        //Sao chép khung nạn nhân sang swap
        __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
        //Sao chép khung mục tiêu từ swap sang mem
        __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

        //Cập nhật bảng trang
        pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

        //Cập nhật trạng thái trực tuyến của trang mục tiêu
        pte_set_fpn(&mm->pgd[pgn], vicfpn);

        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }

    *fpn = PAGING_FPN(mm->pgd[pgn]);
    return 0;
}


/**
 * pg_getval - đọc giá trị tại offset cho trước
 * @mm: vùng nhớ
 * @addr: địa chỉ ảo để truy cập
 * @value: giá trị
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller) {
    int pgn = PAGING_PGN(addr); //Lấy số trang từ địa chỉ ảo
    int off = PAGING_OFFST(addr); //Lấy offset từ địa chỉ ảo
    int fpn; //Số trang vật lý

    //Lấy trang vào MEMRAM, hoán đổi từ MEMSWAP nếu cần
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; //Truy cập trang không hợp lệ

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off; //Tính địa chỉ vật lý

    MEMPHY_read(caller->mram, phyaddr, data); //Đọc từ bộ nhớ vật lý

    return 0; //Trả về 0 nếu thành công
}

/**
 * pg_setval - ghi giá trị vào offset cho trước
 * @mm: vùng nhớ
 * @addr: địa chỉ ảo để truy cập
 * @value: giá trị
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) {
    int pgn = PAGING_PGN(addr); //Lấy số trang từ địa chỉ ảo
    int off = PAGING_OFFST(addr); //Lấy offset từ địa chỉ ảo
    int fpn; //Số trang vật lý

    // Lấy trang vào MEMRAM, hoán đổi từ MEMSWAP nếu cần
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; // Truy cập trang không hợp lệ

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off; //Tính địa chỉ vật lý

    MEMPHY_write(caller->mram, phyaddr, value); //Ghi vào bộ nhớ vật lý

    return 0; //Trả về 0 nếu thành công
}

/**
 * __read - đọc giá trị trong vùng nhớ
 * @caller: người gọi
 * @vmaid: ID của vùng vm để cấp phát vùng nhớ
 * @offset: offset để truy cập trong vùng nhớ
 * @rgid: ID của vùng nhớ (dùng để nhận diện biến trong bảng ký hiệu)
 * @size: kích thước đã cấp phát
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data) {
    pthread_mutex_lock(&mem_lock); //Khóa mutex

    //Lấy vùng nhớ và vùng vm dựa trên ID
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    //Kiểm tra điều kiện không hợp lệ
    if (currg == NULL || cur_vma == NULL) {
        pthread_mutex_unlock(&mem_lock); //Mở khóa mutex
        return -1; //Trả về lỗi nếu nhận dạng không hợp lệ
    }

    //Đọc giá trị từ vùng nhớ
    pg_getval(caller->mm, currg->rg_start + offset, data, caller);

    pthread_mutex_unlock(&mem_lock); //Mở khóa mutex
    return 0; //Trả về 0 nếu thành công
}

/**
 * pgread - Đọc vùng nhớ dựa trên phân trang
 */
int pgread(
    struct pcb_t *proc,          //Quá trình thực thi lệnh
    uint32_t source,             //Chỉ mục của thanh ghi nguồn
    uint32_t offset,             //Địa chỉ nguồn = [source] + [offset]
    uint32_t destination)        //Đích lưu dữ liệu
{
    BYTE data;
    int val = __read(proc, 0, source, offset, &data);  //Đọc dữ liệu

    destination = (uint32_t) data;  //Gán giá trị vào biến đích

    #ifdef IODUMP
    //In thông tin đọc
        printf("read region=%d offset=%d value=%d\n", source, offset, data);
    #ifdef PAGETBL_DUMP
    //In bảng phân trang nếu được cấu hình
        print_pgtbl(proc, 0, -1); //In bảng phân trang tối đa
    #endif
    //Dump bộ nhớ vật lý
        MEMPHY_dump(proc->mram);
    #endif

    return val;  //Trả về kết quả của hàm __read
}

/**
 * __write - ghi vào một vùng nhớ
 * @caller: người gọi hàm
 * @vmaid: ID của khu vực VM để cấp phát vùng nhớ
 * @offset: offset để truy cập trong vùng nhớ
 * @rgid: ID của vùng nhớ (dùng để nhận diện biến trong bảng ký hiệu)
 * @size: kích thước đã cấp phát
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value) {
    pthread_mutex_lock(&mem_lock); //Khóa mutex

    //Lấy vùng nhớ và vùng vm dựa trên ID
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    //Kiểm tra điều kiện không hợp lệ
    if (currg == NULL || cur_vma == NULL) {
        pthread_mutex_unlock(&mem_lock); //Mở khóa mutex
        return -1; //Trả về lỗi nếu nhận dạng không hợp lệ
    }

    //Ghi giá trị vào vùng nhớ
    pg_setval(caller->mm, currg->rg_start + offset, value, caller);

    pthread_mutex_unlock(&mem_lock); //Mở khóa mutex
    return 0; //Trả về 0 nếu thành công
}

/**
 * pgwrite - Ghi vào vùng nhớ dựa trên phân trang
 */
int pgwrite(
    struct pcb_t *proc,            // Quá trình thực thi lệnh
    BYTE data,                     // Dữ liệu để ghi vào bộ nhớ
    uint32_t destination,          // Chỉ mục của thanh ghi đích
    uint32_t offset)               // Offset
{
#ifdef IODUMP
    //In thông tin ghi
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    //In bảng phân trang nếu được cấu hình
    print_pgtbl(proc, 0, -1); //In bảng phân trang tối đa
#endif
    pthread_mutex_lock(&mem_lock);
    //Dump bộ nhớ vật lý
    MEMPHY_dump(proc->mram);
    pthread_mutex_unlock(&mem_lock);
#endif

    return __write(proc, 0, destination, offset, data); //Gọi hàm __write để thực hiện ghi
}

/**
 * free_pcb_memphy - thu hồi tất cả các trang nhớ của pcb
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @incpgnum: số trang
 */
int free_pcb_memph(struct pcb_t *caller) {
    int pagenum, fpn;
    uint32_t pte;

    //Duyệt qua tất cả các trang
    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++) {
        pte = caller->mm->pgd[pagenum]; //Lấy entry từ page directory

        if (!PAGING_PAGE_PRESENT(pte)) {
            //Nếu trang không tồn tại trong bộ nhớ vật lý
            fpn = PAGING_FPN(pte);
            MEMPHY_put_freefp(caller->mram, fpn); //Giải phóng frame vật lý
        } else {
            //Nếu trang được lưu trong bộ nhớ swap
            fpn = PAGING_SWP(pte);
            MEMPHY_put_freefp(caller->active_mswp, fpn); //Giải phóng frame swap
        }
    }

    return 0; //Trả về 0 sau khi hoàn thành
}

/**
 * get_vm_area_node_at_brk - Lấy vùng vm cho một số trang nhất định
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @size: kích thước cần cấp phát
 * @alignedsz: kích thước đã căn chỉnh (không sử dụng trong hàm này)
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz) {
    struct vm_rg_struct *newrg;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); //Lấy vùng vm hiện tại

    newrg = malloc(sizeof(struct vm_rg_struct)); //Cấp phát bộ nhớ cho cấu trúc mới

    newrg->rg_start = cur_vma->sbrk; //Đặt điểm bắt đầu của vùng mới
    newrg->rg_end = newrg->rg_start + size; //Đặt điểm kết thúc của vùng mới

    return newrg; //Trả về cấu trúc vùng mới
}

/**
 * validate_overlap_vm_area - kiểm tra vùng nhớ được lập kế hoạch không bị chồng chéo
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @vmastart: điểm bắt đầu của vùng VM
 * @vmaend: điểm kết thúc của vùng VM
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend) {
    if (caller == NULL)
        return -1; //Trả về lỗi nếu người gọi không tồn tại

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // Lấy vùng VM hiện tại
    if (cur_vma == NULL)
        return -1; //Trả về lỗi nếu không tìm thấy vùng VM

    struct vm_area_struct *iter = caller->mm->mmap; //Duyệt danh sách vùng VM
    while (iter != NULL) {
        //Kiểm tra điều kiện chồng chéo
        if (!(iter == cur_vma || iter->vm_end <= vmastart || vmaend <= iter->vm_start))
            return 1; //Phát hiện chồng chéo
        iter = iter->vm_next; //Tiếp tục duyệt
    }

    return 0; //Không phát hiện chồng chéo
}


/**
 * inc_vma_limit - tăng giới hạn khu vực VM để dành chỗ cho biến mới
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @inc_sz: kích thước tăng thêm
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz) {
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct)); // Cấp phát vùng nhớ mới
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz); //Kích thước tăng đã căn chỉnh theo trang
    int incnumpage = inc_amt / PAGING_PAGESZ; //Số trang tăng thêm
    struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt); //Lấy vùng VM mới
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); //Lấy vùng VM hiện tại

    int old_end = cur_vma->vm_end; //Điểm kết thúc cũ của VM

    //Kiểm tra trùng lặp của vùng nhớ đã lấy
    if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
        return -1; //Trùng lặp và cấp phát thất bại

    //Chỉ vùng VM nhận được
    //bây giờ sẽ được cấp phát thực sự vào RAM
    cur_vma->vm_end += inc_sz;
    cur_vma->sbrk += inc_sz;

    //Ánh xạ bộ nhớ vào MEMRAM
    if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage, newrg) < 0)
        return -1; //Nếu ánh xạ thất bại

    return 0; //Trả về 0 nếu thành công
}

/**
 * find_victim_page - tìm trang nạn nhân
 * @mm: cấu trúc quản lý bộ nhớ
 * @retpgn: trả về số trang
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) {
    struct pgn_t *pg = mm->fifo_pgn; //Lấy trang đầu tiên trong hàng đợi FIFO

    //TODO:
    if (pg == NULL)
        return -1; //Nếu không có trang, trả về lỗi

    if (pg->pg_next == NULL)
        mm->fifo_pgn = NULL; //Nếu chỉ có một trang, xóa hàng đợi
    else {
        struct pgn_t *prev; //Biến để giữ trang trước
        while (pg->pg_next != NULL) {
            prev = pg;
            pg = pg->pg_next; //Di chuyển đến trang cuối cùng
        }
        prev->pg_next = NULL; //Xóa liên kết đến trang cuối cùng
    }
    *retpgn = pg->pgn; //Gán số trang nạn nhân

    free(pg); //Giải phóng bộ nhớ của trang đã chọn

    return 0; //Trả về 0 nếu thành công
}

/**
 * get_free_vmrg_area - lấy một khu vực VM trống
 * @caller: người gọi
 * @vmaid: ID khu vực VM để cấp phát vùng nhớ
 * @size: kích thước được cấp phát
 * @newrg: vùng VM mới
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); //Lấy vùng VM hiện tại
    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list; //Danh sách vùng VM trống

    if (rgit == NULL)
        return -1; //Không có vùng trống, trả về lỗi

    //Khởi tạo newrg không được gán giá trị
    newrg->rg_start = newrg->rg_end = -1;

    //Duyệt danh sách các vùng VM trống để tìm không gian phù hợp
    while (rgit != NULL) {
        if (rgit->rg_start + size <= rgit->rg_end) { //Vùng hiện tại có đủ không gian
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            //Cập nhật không gian còn lại trong vùng đã chọn
            if (rgit->rg_start + size < rgit->rg_end)
                rgit->rg_start += size;
            else { //Sử dụng hết không gian, loại bỏ nút hiện tại
                struct vm_rg_struct *nextrg = rgit->rg_next;// Nút tiếp theo

                // Sao chép thông tin từ nút tiếp theo nếu có
                if (nextrg != NULL) {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;
                    rgit->rg_next = nextrg->rg_next;
                    free(nextrg);
                } else { //Cuối danh sách trống
                    rgit->rg_start = rgit->rg_end; //dummy, khu vực kích thước 0
                    rgit->rg_next = NULL;
                }
            }
            break;
        } else rgit = rgit->rg_next; //Duyệt nút tiếp theo
        
    }
    if (newrg->rg_start == -1) //Không tìm thấy vùng mới
        return -1;

    return 0; //Trả về 0 nếu thành công
}

#endif