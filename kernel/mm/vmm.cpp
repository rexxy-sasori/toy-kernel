#include "vmm.h"
#include "../lib/addr.h"
#include "pmm.h"

static page_table_t *get_page_table(page_table_t *root, uint64_t index, bool create) {
    auto entry = &root->get(index);

    if (entry->get_flags() & PAGE_TABLE_ENTRY_PRESENT)
        return (page_table_t *) phys_to_io(entry->get_address());

    if (!create)
        return nullptr;

    auto pt_phys = pmm::alloc(1);
    auto pt = (page_table_t *) phys_to_io(pt_phys);

    __builtin_memset(pt, 0, sizeof(page_table_t));

    entry->set_address(pt_phys);
    entry->set_flags(PAGE_TABLE_ENTRY_PRESENT | PAGE_TABLE_ENTRY_WRITE);

    return pt;
}

static void map_page(page_table_t *root, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    auto pml3 = get_page_table(root, (virt_addr >> 39) & 0x1ff, true);
    auto pml2 = get_page_table(pml3, (virt_addr >> 30) & 0x1ff, true);
    auto pml1 = get_page_table(pml2, (virt_addr >> 21) & 0x1ff, true);
    auto entry = &pml1->get((virt_addr >> 12) & 0x1ff);

    if (entry->get_flags() & PAGE_TABLE_ENTRY_PRESENT)
        return; // TODO: Panic - address is already mapped

    entry->set_address(phys_addr);
    entry->set_flags(flags);
}

static void unmap_page(page_table_t *root, uint64_t virt_addr) {
    auto pml3 = get_page_table(root, (virt_addr >> 39) & 0x1ff, false);
    auto pml2 = pml3 ? get_page_table(pml3, (virt_addr >> 30) & 0x1ff, false) : nullptr;
    auto pml1 = pml2 ? get_page_table(pml2, (virt_addr >> 21) & 0x1ff, false) : nullptr;
    auto entry = pml1 ? &pml1->get((virt_addr >> 12) & 0x1ff) : nullptr;

    if (!entry || !(entry->get_flags() & PAGE_TABLE_ENTRY_PRESENT))
        return; // TODO: Panic - address is not mapped

    entry->set_address(0);
    entry->set_flags(0);

    asm("invlpg (%0)" : : "r"(virt_addr));
}

void page_table_t::map(uint64_t virt_addr, uint64_t phys_addr, uint64_t size, uint64_t flags) {
    for (auto i = 0; i < size / 4096; i++) {
        map_page(this, virt_addr + i * 4096, phys_addr + i * 4096, flags);
    }
}

void page_table_t::unmap(uint64_t virt_addr, uint64_t size) {
    for (auto i = 0; i < size / 4096; i++) {
        unmap_page(this, virt_addr + i * 4096);
    }
}

void vmm::init(stivale2_struct_pmrs_tag_t *pmrs) {
    auto pt_phys = pmm::alloc(1);
    auto pt = (page_table_t *) phys_to_io(pt_phys);

    __builtin_memset(pt, 0, sizeof(page_table_t));

    pt->map(0, 0, gib(4), PAGE_TABLE_ENTRY_PRESENT | PAGE_TABLE_ENTRY_WRITE);
    pt->map(phys_to_io(0), 0, gib(4), PAGE_TABLE_ENTRY_PRESENT | PAGE_TABLE_ENTRY_WRITE);

    pt->unmap(0, 4096);

    for (auto i = 0; i < pmrs->count; i++) {
        auto pmr = &pmrs->pmrs[i];
        auto flags = (uint64_t) PAGE_TABLE_ENTRY_PRESENT;

        if (pmr->permissions & STIVALE2_PMR_WRITABLE)
            flags |= PAGE_TABLE_ENTRY_WRITE;

        if (!(pmr->permissions & STIVALE2_PMR_EXECUTABLE))
            flags |= PAGE_TABLE_ENTRY_NO_EXECUTE;

        pt->map(pmr->base, kernel_to_phys(pmr->base), align_up(pmr->size, 4096), flags);
    }

    kernel_pt = pt;

    asm("mov %0, %%cr3" : : "r"(pt_phys));
}