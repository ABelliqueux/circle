//
// bcmpciehostbridge.cpp
//
// This driver has been ported from the Linux driver which is:
//	drivers/pci/controller/pcie-brcmstb.c
//	Copyright (C) 2009 - 2019 Broadcom
//	Licensed under GPL-2.0+
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2019-2025  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/bcmpciehostbridge.h>
#include <circle/memio.h>
#include <circle/bcm2711.h>
#include <circle/bcm2712.h>
#include <circle/pci_regs.h>
#include <circle/logger.h>
#include <circle/sysconfig.h>
#include <circle/machineinfo.h>
#include <circle/util.h>
#include <circle/macros.h>
#include <circle/debug.h>
#include <assert.h>

#define PCIE_GEN	2

/* BRCM_PCIE_CAP_REGS - Offset for the mandatory capability config regs */
#define BRCM_PCIE_CAP_REGS				0x00ac

/*
 * Broadcom Settop Box PCIe Register Offsets. The names are from
 * the chip's RDB and we use them here so that a script can correlate
 * this code and the RDB to prevent discrepancies.
 */
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1		0x0188
#define PCIE_RC_CFG_PRIV1_ID_VAL3			0x043c
#define PCIE_RC_DL_MDIO_ADDR				0x1100
#define PCIE_RC_DL_MDIO_WR_DATA				0x1104
#define PCIE_RC_DL_MDIO_RD_DATA				0x1108
#define PCIE_MISC_MISC_CTRL				0x4008
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LO		0x400c
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_HI		0x4010
#define PCIE_MISC_RC_BAR1_CONFIG_LO			0x402c
#define PCIE_MISC_RC_BAR1_CONFIG_HI			0x4030
#define PCIE_MISC_RC_BAR2_CONFIG_LO			0x4034
#define PCIE_MISC_RC_BAR2_CONFIG_HI			0x4038
#define PCIE_MISC_RC_BAR3_CONFIG_LO			0x403c
#define PCIE_MISC_MSI_BAR_CONFIG_LO			0x4044
#define PCIE_MISC_MSI_BAR_CONFIG_HI			0x4048
#define PCIE_MISC_MSI_DATA_CONFIG			0x404c
#define PCIE_MISC_EOI_CTRL				0x4060
#define PCIE_MISC_PCIE_CTRL				0x4064
#define PCIE_MISC_PCIE_STATUS				0x4068
#define PCIE_MISC_REVISION				0x406c
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT	0x4070
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_HI		0x4080
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LIMIT_HI		0x4084
#if RASPPI == 4
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG			0x4204
#define PCIE_INTR2_CPU_BASE				0x4300
#else
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG			0x4304
#define PCIE_INTR2_CPU_BASE				0x4400
#endif
#define PCIE_MSI_INTR2_BASE				0x4500


#if RASPPI >= 5

#define  PCIE_RC_CFG_VENDOR_SPCIFIC_REG1_LITTLE_ENDIAN		0x0

#define PCIE_RC_CFG_PRIV1_LINK_CAPABILITY			0x04dc
#define  PCIE_RC_CFG_PRIV1_LINK_CAPABILITY_ASPM_SUPPORT_MASK 	0xc00
#define  PCIE_RC_CFG_PRIV1_LINK_CAPABILITY_ASPM_SUPPORT_SHIFT 	0xa
#define   PCIE_LINK_STATE_L0S					BIT(0)
#define   PCIE_LINK_STATE_L1					BIT(1)

#define PCIE_RC_TL_VDM_CTL1					0x0a0c
#define  PCIE_RC_TL_VDM_CTL1_VDM_VNDRID0_MASK			0x0000ffff
#define  PCIE_RC_TL_VDM_CTL1_VDM_VNDRID1_MASK			0xffff0000

#define PCIE_RC_TL_VDM_CTL0					0x0a20
#define  PCIE_RC_TL_VDM_CTL0_VDM_ENABLED_MASK			0x10000
#define  PCIE_RC_TL_VDM_CTL0_VDM_IGNORETAG_MASK			0x20000
#define  PCIE_RC_TL_VDM_CTL0_VDM_IGNOREVNDRID_MASK		0x40000

#define PCIE_RC_PL_PHY_CTL_15					0x184c
#define  PCIE_RC_PL_PHY_CTL_15_DIS_PLL_PD_MASK			0x400000
#define  PCIE_RC_PL_PHY_CTL_15_PM_CLK_PERIOD_MASK		0xff

#define PCIE_MISC_RC_CONFIG_RETRY_TIMEOUT			0x405c

#define PCIE_MISC_CTRL_1					0x40a0
#define  PCIE_MISC_CTRL_1_OUTBOUND_TC_MASK			0xf
#define  PCIE_MISC_CTRL_1_OUTBOUND_NO_SNOOP_MASK		BIT(3)
#define  PCIE_MISC_CTRL_1_OUTBOUND_RO_MASK			BIT(4)
#define  PCIE_MISC_CTRL_1_EN_VDM_QOS_CONTROL_MASK		BIT(5)

#define PCIE_MISC_UBUS_CTRL					0x40a4
#define  PCIE_MISC_UBUS_CTRL_UBUS_PCIE_REPLY_ERR_DIS_MASK	BIT(13)
#define  PCIE_MISC_UBUS_CTRL_UBUS_PCIE_REPLY_ERR_DIS_SHIFT	13
#define  PCIE_MISC_UBUS_CTRL_UBUS_PCIE_REPLY_DECERR_DIS_MASK	BIT(19)
#define  PCIE_MISC_UBUS_CTRL_UBUS_PCIE_REPLY_DECERR_DIS_SHIFT	19

#define PCIE_MISC_UBUS_TIMEOUT					0x40A8

#define PCIE_MISC_UBUS_BAR1_CONFIG_REMAP			0x40ac
#define  PCIE_MISC_UBUS_BAR1_CONFIG_REMAP_ACCESS_ENABLE_MASK	BIT(0)
#define PCIE_MISC_UBUS_BAR1_CONFIG_REMAP_HI			0x40b0

#define PCIE_MISC_UBUS_BAR2_CONFIG_REMAP			0x40b4
#define  PCIE_MISC_UBUS_BAR2_CONFIG_REMAP_ACCESS_ENABLE_MASK	BIT(0)
#define  PCIE_MISC_UBUS_BAR2_CONFIG_REMAP_ACCESS_ENABLE_SHIFT	0x0

/* VDM messages indexing TCs to AXI priorities */
/* Indexes 8-15 */
#define PCIE_MISC_VDM_PRIORITY_TO_QOS_MAP_HI			0x4164
/* Indexes 0-7 */
#define PCIE_MISC_VDM_PRIORITY_TO_QOS_MAP_LO			0x4168
#define  VDM_PRIORITY_TO_QOS_MAP_SHIFT(x)			(4 * (x))
#define  VDM_PRIORITY_TO_QOS_MAP_MASK				0xf

#define PCIE_MISC_AXI_INTF_CTRL 				0x416c
#define  AXI_REQFIFO_EN_QOS_PROPAGATION				BIT(7)
#define  AXI_BRIDGE_LOW_LATENCY_MODE				BIT(6)
#define  AXI_MASTER_MAX_OUTSTANDING_REQUESTS_MASK		0x3f

#define PCIE_MISC_AXI_READ_ERROR_DATA				0x4170

#endif	// #if RASPPI >= 5


/*
 * Broadcom Settop Box PCIe Register Field shift and mask info. The
 * names are from the chip's RDB and we use them here so that a script
 * can correlate this code and the RDB to prevent discrepancies.
 */
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_MASK	0xc
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_SHIFT	0x2
#define PCIE_RC_CFG_PRIV1_ID_VAL3_CLASS_CODE_MASK		0xffffff
#define PCIE_RC_CFG_PRIV1_ID_VAL3_CLASS_CODE_SHIFT		0x0
#define PCIE_MISC_MISC_CTRL_RCB_MPS_MODE_MASK			0x400
#define PCIE_MISC_MISC_CTRL_RCB_MPS_MODE_SHIFT			0xa
#define PCIE_MISC_MISC_CTRL_SCB_ACCESS_EN_MASK			0x1000
#define PCIE_MISC_MISC_CTRL_SCB_ACCESS_EN_SHIFT			0xc
#define PCIE_MISC_MISC_CTRL_CFG_READ_UR_MODE_MASK		0x2000
#define PCIE_MISC_MISC_CTRL_CFG_READ_UR_MODE_SHIFT		0xd
#define PCIE_MISC_MISC_CTRL_MAX_BURST_SIZE_MASK			0x300000
#define PCIE_MISC_MISC_CTRL_MAX_BURST_SIZE_SHIFT		0x14
#define PCIE_MISC_MISC_CTRL_SCB0_SIZE_MASK			0xf8000000
#define PCIE_MISC_MISC_CTRL_SCB0_SIZE_SHIFT			0x1b
#define PCIE_MISC_MISC_CTRL_SCB1_SIZE_MASK			0x7c00000
#define PCIE_MISC_MISC_CTRL_SCB1_SIZE_SHIFT			0x16
#define PCIE_MISC_MISC_CTRL_SCB2_SIZE_MASK			0x1f
#define PCIE_MISC_MISC_CTRL_SCB2_SIZE_SHIFT			0x0
#define PCIE_MISC_RC_BAR1_CONFIG_LO_SIZE_MASK			0x1f
#define PCIE_MISC_RC_BAR1_CONFIG_LO_SIZE_SHIFT			0x0
#define PCIE_MISC_RC_BAR2_CONFIG_LO_SIZE_MASK			0x1f
#define PCIE_MISC_RC_BAR2_CONFIG_LO_SIZE_SHIFT			0x0
#define PCIE_MISC_RC_BAR3_CONFIG_LO_SIZE_MASK			0x1f
#define PCIE_MISC_RC_BAR3_CONFIG_LO_SIZE_SHIFT			0x0
#define PCIE_MISC_PCIE_CTRL_PCIE_PERSTB_MASK			0x4
#define PCIE_MISC_PCIE_CTRL_PCIE_PERSTB_SHIFT			0x2
#define PCIE_MISC_PCIE_CTRL_PCIE_L23_REQUEST_MASK		0x1
#define PCIE_MISC_PCIE_CTRL_PCIE_L23_REQUEST_SHIFT		0x0
#define PCIE_MISC_PCIE_STATUS_PCIE_PORT_MASK			0x80
#define PCIE_MISC_PCIE_STATUS_PCIE_PORT_SHIFT			0x7
#define PCIE_MISC_PCIE_STATUS_PCIE_DL_ACTIVE_MASK		0x20
#define PCIE_MISC_PCIE_STATUS_PCIE_DL_ACTIVE_SHIFT		0x5
#define PCIE_MISC_PCIE_STATUS_PCIE_PHYLINKUP_MASK		0x10
#define PCIE_MISC_PCIE_STATUS_PCIE_PHYLINKUP_SHIFT		0x4
#define PCIE_MISC_PCIE_STATUS_PCIE_LINK_IN_L23_MASK		0x40
#define PCIE_MISC_PCIE_STATUS_PCIE_LINK_IN_L23_SHIFT		0x6
#define PCIE_MISC_REVISION_MAJMIN_MASK				0xffff
#define PCIE_MISC_REVISION_MAJMIN_SHIFT				0
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_LIMIT_MASK	0xfff00000
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_LIMIT_SHIFT	0x14
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_BASE_MASK	0xfff0
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_BASE_SHIFT	0x4
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_NUM_MASK_BITS	0xc
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_HI_BASE_MASK		0xff
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_HI_BASE_SHIFT	0x0
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LIMIT_HI_LIMIT_MASK	0xff
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LIMIT_HI_LIMIT_SHIFT	0x0
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_DEBUG_ENABLE_MASK	0x2
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_DEBUG_ENABLE_SHIFT 0x1
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ_MASK		0x08000000
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ_SHIFT	0x1b
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_L1SS_ENABLE_MASK	0x00200000
#define PCIE_RGR1_SW_INIT_1_PERST_MASK				0x1
#define PCIE_RGR1_SW_INIT_1_PERST_SHIFT				0x0

#define BRCM_INT_PCI_MSI_NR		32
#define BRCM_PCIE_HW_REV_33		0x0303

#define BRCM_MSI_TARGET_ADDR_LT_4GB	0x0fffffffcULL
#define BRCM_MSI_TARGET_ADDR_GT_4GB	0xffffffffcULL

#define BURST_SIZE_128			0
#define BURST_SIZE_256			1
#define BURST_SIZE_512			2

/* Offsets from PCIE_INTR2_CPU_BASE */
#define STATUS				0x0
#define SET				0x4
#define CLR				0x8
#define MASK_STATUS			0xc
#define MASK_SET			0x10
#define MASK_CLR			0x14

#define PCIE_BUSNUM_SHIFT		20
#define PCIE_SLOT_SHIFT			15
#define PCIE_FUNC_SHIFT			12

#define	DATA_ENDIAN			0
#define MMIO_ENDIAN			0

#define MDIO_PORT0			0x0
#define MDIO_DATA_MASK			0x7fffffff
#define MDIO_DATA_SHIFT			0x0
#define MDIO_PORT_MASK			0xf0000
#define MDIO_PORT_SHIFT			0x16
#define MDIO_REGAD_MASK			0xffff
#define MDIO_REGAD_SHIFT		0x0
#define MDIO_CMD_MASK			0xfff00000
#define MDIO_CMD_SHIFT			0x14
#define MDIO_CMD_READ			0x1
#define MDIO_CMD_WRITE			0x0
#define MDIO_DATA_DONE_MASK		0x80000000
#define MDIO_RD_DONE(x)			(((x) & MDIO_DATA_DONE_MASK) ? 1 : 0)
#define MDIO_WT_DONE(x)			(((x) & MDIO_DATA_DONE_MASK) ? 0 : 1)
#define SSC_REGS_ADDR			0x1100
#define SET_ADDR_OFFSET			0x1f
#define SSC_CNTL_OFFSET			0x2
#define SSC_CNTL_OVRD_EN_MASK		0x8000
#define SSC_CNTL_OVRD_EN_SHIFT		0xf
#define SSC_CNTL_OVRD_VAL_MASK		0x4000
#define SSC_CNTL_OVRD_VAL_SHIFT		0xe
#define SSC_STATUS_OFFSET		0x1
#define SSC_STATUS_SSC_MASK		0x400
#define SSC_STATUS_SSC_SHIFT		0xa
#define SSC_STATUS_PLL_LOCK_MASK	0x800
#define SSC_STATUS_PLL_LOCK_SHIFT	0xb

#define PCIE_RGR1_SW_INIT_1		0x9210
#define  RGR1_SW_INIT_1_INIT_GENERIC_MASK  0x2
#define  RGR1_SW_INIT_1_INIT_GENERIC_SHIFT 0x1
#define PCIE_EXT_CFG_INDEX		0x9000
#define PCIE_EXT_CFG_DATA		0x8000

#define bcm_readl(a)		read32(a)
#define bcm_writel(d, a)	write32(a, d)
#define bcm_readw(a)		read16(a)
#define bcm_writew(d, a)	write16(a, d)

/* These macros extract/insert fields to host controller's register set. */
#define RD_FLD(m_base, reg, field) \
	rd_fld(m_base + reg, reg##_##field##_MASK, reg##_##field##_SHIFT)
#define WR_FLD(m_base, reg, field, val) \
	wr_fld(m_base + reg, reg##_##field##_MASK, reg##_##field##_SHIFT, val)
#define WR_FLD_RB(m_base, reg, field, val) \
	wr_fld_rb(m_base + reg, reg##_##field##_MASK, reg##_##field##_SHIFT, val)
#define WR_FLD_WITH_OFFSET(m_base, off, reg, field, val) \
	wr_fld(m_base + reg + off, reg##_##field##_MASK, reg##_##field##_SHIFT, val)
#define EXTRACT_FIELD(val, reg, field) \
	((val & reg##_##field##_MASK) >> reg##_##field##_SHIFT)
#define INSERT_FIELD(val, reg, field, field_val) \
	((val & ~reg##_##field##_MASK) | \
	 (reg##_##field##_MASK & (field_val << reg##_##field##_SHIFT)))

#define PCI_BUS(n)		(n)

#define PCI_DEVFN(slot, func)	((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

#define lower_32_bits(v)	((v) & 0xFFFFFFFFU)
#define upper_32_bits(v)	((v) >> 32)

u64 CBcmPCIeHostBridge::s_nDMAAddress[PCIE_BUS_NUM] = {0};

static const char FromPCIeHost[] = "pcie";

CBcmPCIeHostBridge::CBcmPCIeHostBridge (unsigned nBus, CInterruptSystem *pInterrupt)
:	m_nBus (nBus),
	m_pInterrupt (pInterrupt),
#if RASPPI == 4
	m_base (ARM_PCIE_HOST_BASE),
#else
	m_base (m_nBus == PCIE_BUS_ONBOARD ? ARM_PCIE_HOST_BASE : ARM_PCIE_EXT_HOST_BASE),
#endif
	m_num_out_wins (0),
	m_num_dma_ranges (0),
	m_num_scbs (0),
	m_msi (0)
{
}

CBcmPCIeHostBridge::~CBcmPCIeHostBridge (void)
{
#if 0
	if (m_msi != 0)
	{
		DisconnectMSI ();
	}
#endif

	m_pInterrupt = 0;
}

boolean CBcmPCIeHostBridge::Initialize (void)
{
	int ret = pcie_probe();
	if (ret)
	{
		CLogger::Get ()->Write (FromPCIeHost, LogError,
					"Cannot initialize PCIe bridge (%d)", ret);

		return FALSE;
	}

	ret = enable_bridge();
	if (ret)
	{
		CLogger::Get ()->Write (FromPCIeHost, LogError,
					"Cannot enable PCIe bridge (%d)", ret);

		return FALSE;
	}

	return TRUE;
}

boolean CBcmPCIeHostBridge::EnableDevice (u32 nClassCode, unsigned nSlot, unsigned nFunc)
{
	return !enable_device (nClassCode, nSlot, nFunc);
}

#if 0

boolean CBcmPCIeHostBridge::ConnectMSI (TPCIeMSIHandler *pHandler, void *pParam)
{
	return !pcie_enable_msi (pHandler, pParam);
}

void CBcmPCIeHostBridge::DisconnectMSI (void)
{
	bcm_writel(0xffffffff, m_msi->intr_base + MASK_SET);
	bcm_writel(0, m_msi->base + PCIE_MISC_MSI_BAR_CONFIG_LO);

	assert (m_pInterrupt != 0);
	m_pInterrupt->DisconnectIRQ (ARM_IRQ_PCIE_HOST_MSI);

	delete m_msi;
	m_msi = 0;
}

#endif

#ifndef NDEBUG

void CBcmPCIeHostBridge::DumpStatus (unsigned nSlot, unsigned nFunc)
{
	uintptr conf = pcie_map_conf (PCI_BUS (0), PCI_DEVFN(0, 0), 0);
	CLogger::Get ()->Write (FromPCIeHost, LogDebug, "Bridge status is 0x%X",
				conf ? (unsigned) read16 (conf + PCI_STATUS) : -1);

	assert (nSlot < 32);
	assert (nFunc < 8);
	conf = pcie_map_conf (PCI_BUS (1), PCI_DEVFN (nSlot, nFunc), 0);
	CLogger::Get ()->Write (FromPCIeHost, LogDebug, "Device status is 0x%X",
				conf ? (unsigned) read16 (conf + PCI_STATUS) : -1);
}

#endif

int CBcmPCIeHostBridge::pcie_probe(void)
{
	// TODO? DMA target addresses >= 0xC0000000 may need bounce support

	int ret = pcie_set_pci_ranges();
	if (ret)
		return ret;

	ret = pcie_set_dma_ranges();
	if (ret)
		return ret;

	return pcie_setup();
}

int CBcmPCIeHostBridge::pcie_setup(void)
{
	unsigned int scb_size_val;
	u64 rc_bar2_offset, rc_bar2_size, total_mem_size = 0;
	u32 tmp;
	int i, j, limit;

#if RASPPI >= 5
	if (m_nBus == PCIE_BUS_ONBOARD)	// only done once for bridge, which is init first
	{
		int ret = rescal_reset_deassert();
		if (ret)
			return ret;
	}
#endif

	/* Reset the bridge */
	pcie_bridge_sw_init_set(1);

#if RASPPI == 4
	/* Ensure that the fundamental reset is asserted */
	pcie_perst_set(1);
#endif

	usleep_range(100, 200);

	/* Take the bridge out of reset */
	pcie_bridge_sw_init_set(0);

	WR_FLD_RB(m_base, PCIE_MISC_HARD_PCIE_HARD_DEBUG, SERDES_IDDQ, 0);
	/* Wait for SerDes to be stable */
	usleep_range(100, 200);

	/* Grab the PCIe hw revision number */
	tmp = bcm_readl(m_base + PCIE_MISC_REVISION);
	m_rev = EXTRACT_FIELD(tmp, PCIE_MISC_REVISION, MAJMIN);

#if RASPPI >= 5
	/* Allow a 54MHz (xosc) refclk source */
	pcie_munge_pll();
	/* Fix for L1SS errata */
	tmp = bcm_readl(m_base + PCIE_RC_PL_PHY_CTL_15);
	tmp &= ~PCIE_RC_PL_PHY_CTL_15_PM_CLK_PERIOD_MASK;
	/* PM clock period is 18.52ns (round down) */
	tmp |= 0x12;
	bcm_writel(tmp, m_base + PCIE_RC_PL_PHY_CTL_15);
#endif

	/* Set SCB_MAX_BURST_SIZE, CFG_READ_UR_MODE, SCB_ACCESS_EN, RCB_MPS_MODE */
	tmp = bcm_readl(m_base + PCIE_MISC_MISC_CTRL);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_MISC_CTRL, SCB_ACCESS_EN, 1);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_MISC_CTRL, CFG_READ_UR_MODE, 1);
#if RASPPI == 4
	tmp = INSERT_FIELD(tmp, PCIE_MISC_MISC_CTRL, MAX_BURST_SIZE, BURST_SIZE_128);
#else
	tmp = INSERT_FIELD(tmp, PCIE_MISC_MISC_CTRL, MAX_BURST_SIZE, BURST_SIZE_256);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_MISC_CTRL, RCB_MPS_MODE, 1);
#endif
	bcm_writel(tmp, m_base + PCIE_MISC_MISC_CTRL);

#if RASPPI >= 5
	pcie_set_tc_qos();
#endif

	/*
	 * Set up inbound memory view for the EP (called RC_BAR2,
	 * not to be confused with the BARs that are advertised by
	 * the EP).
	 *
	 * The PCIe host controller by design must set the inbound
	 * viewport to be a contiguous arrangement of all of the
	 * system's memory.  In addition, its size mut be a power of
	 * two.  Further, the MSI target address must NOT be placed
	 * inside this region, as the decoding logic will consider its
	 * address to be inbound memory traffic.  To further
	 * complicate matters, the viewport must start on a
	 * pcie-address that is aligned on a multiple of its size.
	 * If a portion of the viewport does not represent system
	 * memory -- e.g. 3GB of memory requires a 4GB viewport --
	 * we can map the outbound memory in or after 3GB and even
	 * though the viewport will overlap the outbound memory
	 * the controller will know to send outbound memory downstream
	 * and everything else upstream.
	 */
	assert (m_num_dma_ranges == 1);
	m_scb_size[0] = m_dma_ranges[0].size;		// must be a power of 2
	m_num_scbs = m_num_dma_ranges;
	rc_bar2_offset = m_dma_ranges[0].pcie_addr;

	assert (m_num_scbs == 1);
	total_mem_size = m_scb_size[0];
	rc_bar2_size = total_mem_size;			// must be a power of 2

	/* Verify the alignment is correct */
	assert (!(rc_bar2_offset & (rc_bar2_size - 1)));

#if RASPPI == 4
	/*
	 * Position the MSI target low if possible.
	 *
	 * TO DO: Consider outbound window when choosing MSI target and
	 * verifying configuration.
	 */
	u64 msi_target_addr = BRCM_MSI_TARGET_ADDR_LT_4GB;
	if (   rc_bar2_offset <= msi_target_addr
	    && rc_bar2_offset + rc_bar2_size > msi_target_addr)
		msi_target_addr = BRCM_MSI_TARGET_ADDR_GT_4GB;

	m_msi_target_addr = msi_target_addr;
#endif

	tmp = lower_32_bits(rc_bar2_offset);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_RC_BAR2_CONFIG_LO, SIZE, encode_ibar_size(rc_bar2_size));
	bcm_writel(tmp, m_base + PCIE_MISC_RC_BAR2_CONFIG_LO);
	bcm_writel(upper_32_bits(rc_bar2_offset), m_base + PCIE_MISC_RC_BAR2_CONFIG_HI);

#if RASPPI >= 5
	tmp = bcm_readl(m_base + PCIE_MISC_UBUS_BAR2_CONFIG_REMAP);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_UBUS_BAR2_CONFIG_REMAP, ACCESS_ENABLE, 1);
	bcm_writel(tmp, m_base + PCIE_MISC_UBUS_BAR2_CONFIG_REMAP);
#endif

	scb_size_val = m_scb_size[0] ? ilog2(m_scb_size[0]) - 15 : 0xf; /* 0xf is 1GB */
	WR_FLD(m_base, PCIE_MISC_MISC_CTRL, SCB0_SIZE, scb_size_val);
	assert (m_num_scbs == 1);	// do not set fields SCB1_SIZE and SCB2_SIZE

#if RASPPI >= 5
	/* Suppress AXI error responses and return 1s for read failures */
	tmp = bcm_readl(m_base + PCIE_MISC_UBUS_CTRL);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_UBUS_CTRL, UBUS_PCIE_REPLY_ERR_DIS, 1);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_UBUS_CTRL, UBUS_PCIE_REPLY_DECERR_DIS, 1);
	bcm_writel(tmp, m_base + PCIE_MISC_UBUS_CTRL);
	bcm_writel(0xffffffff, m_base + PCIE_MISC_AXI_READ_ERROR_DATA);

	/*
	 * Adjust timeouts. The UBUS timeout also affects CRS
	 * completion retries, as the request will get terminated if
	 * either timeout expires, so both have to be a large value
	 * (in clocks of 750MHz).
	 * Set UBUS timeout to 250ms, then set RC config retry timeout
	 * to be ~240ms.
	 *
	 * Setting CRSVis=1 will stop the core from blocking on a CRS
	 * response, but does require the device to be well-behaved...
	 */
	bcm_writel(0xB2D0000, m_base + PCIE_MISC_UBUS_TIMEOUT);
	bcm_writel(0xABA0000, m_base + PCIE_MISC_RC_CONFIG_RETRY_TIMEOUT);

	/*
	 * We ideally want the MSI target address to be located in the 32bit
	 * addressable memory area. Some devices might depend on it. This is
	 * possible either when the inbound window is located above the lower
	 * 4GB or when the inbound area is smaller than 4GB (taking into
	 * account the rounding-up we're forced to perform).
	 */
	if (rc_bar2_offset >= 4*GIGABYTE || (rc_bar2_size + rc_bar2_offset) < 4*GIGABYTE)
		m_msi_target_addr = BRCM_MSI_TARGET_ADDR_LT_4GB;
	else
		m_msi_target_addr = BRCM_MSI_TARGET_ADDR_GT_4GB;
#endif

	/* disable the PCIe->GISB memory window (RC_BAR1) */
	WR_FLD(m_base, PCIE_MISC_RC_BAR1_CONFIG_LO, SIZE, 0);

	/* disable the PCIe->SCB memory window (RC_BAR3) */
	WR_FLD(m_base, PCIE_MISC_RC_BAR3_CONFIG_LO, SIZE, 0);

#if RASPPI == 4
	/* clear any interrupts we find on boot */
	bcm_writel(0xffffffff, m_base + PCIE_INTR2_CPU_BASE + CLR);
	(void)bcm_readl(m_base + PCIE_INTR2_CPU_BASE + CLR);

	/* Mask all interrupts since we are not handling any yet */
	bcm_writel(0xffffffff, m_base + PCIE_INTR2_CPU_BASE + MASK_SET);
	(void)bcm_readl(m_base + PCIE_INTR2_CPU_BASE + MASK_SET);
#endif

#if RASPPI >= 5
	/* Don't advertise L0s capability if 'aspm-no-l0s' */
	u32 aspm_support = PCIE_LINK_STATE_L1;
	aspm_support |= PCIE_LINK_STATE_L0S;
	tmp = bcm_readl(m_base + PCIE_RC_CFG_PRIV1_LINK_CAPABILITY);
	tmp = INSERT_FIELD(tmp, PCIE_RC_CFG_PRIV1_LINK_CAPABILITY, ASPM_SUPPORT, aspm_support);
	bcm_writel(tmp, m_base + PCIE_RC_CFG_PRIV1_LINK_CAPABILITY);
#endif

	set_gen(m_base, PCIE_GEN);

#if RASPPI >= 5
	/*
	 * For config space accesses on the RC, show the right class for
	 * a PCIe-PCIe bridge (the default setting is to be EP mode).
	 */
	tmp = bcm_readl(m_base + PCIE_RC_CFG_PRIV1_ID_VAL3);
	tmp = INSERT_FIELD(tmp, PCIE_RC_CFG_PRIV1_ID_VAL3, CLASS_CODE, 0x060400);
	bcm_writel(tmp, m_base + PCIE_RC_CFG_PRIV1_ID_VAL3);

	for (i = 0; i < m_num_out_wins; i++)
		pcie_set_outbound_win(i, m_out_wins[i].cpu_addr,
					 m_out_wins[i].pcie_addr,
					 m_out_wins[i].size);

	/* PCIe->SCB endian mode for BAR */
	tmp = bcm_readl(m_base + PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1);
	tmp = INSERT_FIELD(tmp, PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1, ENDIAN_MODE_BAR2,
			   PCIE_RC_CFG_VENDOR_SPCIFIC_REG1_LITTLE_ENDIAN);
	bcm_writel(tmp, m_base + PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1);
#endif

	// Back in probe:

#if RASPPI >= 5
	// TODO? enable msi-controller@130000 (mip0)

	/* Use RC_BAR1 for MIP access */	// From DTB:
	u64 msi_pci_addr = 0xFFFFFFF000UL;	// brcm,msi-pci-addr = <0xff 0xfffff000>;
	u64 msi_phys_addr = 0x1000130000UL;	// reg = <0x10 0x00130000 0x0 0xc0>;

	bcm_writel(lower_32_bits(msi_pci_addr) | encode_ibar_size(0x1000),
		m_base + PCIE_MISC_RC_BAR1_CONFIG_LO);
	bcm_writel(upper_32_bits(msi_pci_addr),
		m_base + PCIE_MISC_RC_BAR1_CONFIG_HI);

	bcm_writel(lower_32_bits(msi_phys_addr) |
		PCIE_MISC_UBUS_BAR1_CONFIG_REMAP_ACCESS_ENABLE_MASK,
		m_base + PCIE_MISC_UBUS_BAR1_CONFIG_REMAP);
	bcm_writel(upper_32_bits(msi_phys_addr),
		m_base + PCIE_MISC_UBUS_BAR1_CONFIG_REMAP_HI);
#endif

	// Start link follows:

	/* Unassert the fundamental reset */
	pcie_perst_set(0);

	/*
	 * Wait for 100ms after PERST# deassertion; see PCIe CEM specification
	 * sections 2.2, PCIe r5.0, 6.6.1.
	 */
	msleep(100);

	/*
	 * Give the RC/EP time to wake up, before trying to configure RC.
	 * Intermittently check status for link-up, up to a total of 100ms
	 * when we don't know if the device is there, and up to 1000ms if
	 * we do know the device is there.
	 */
	limit = 100;
	for (i = 1, j = 0; j < limit && !pcie_link_up();
	     j += i, i = i * 2)
		msleep(i + j > limit ? limit - j : i);

	if (!pcie_link_up()) {
		CLogger::Get ()->Write (FromPCIeHost, LogError, "Link down");
		return -1;
	}

	if (!pcie_rc_mode()) {
		CLogger::Get ()->Write (FromPCIeHost, LogError, "PCIe misconfigured; is in EP mode");
		return -1;
	}

#if RASPPI == 4
	// This was done earlier for RASPPI >= 5
	for (i = 0; i < m_num_out_wins; i++)
		pcie_set_outbound_win(i, m_out_wins[i].cpu_addr,
					 m_out_wins[i].pcie_addr,
					 m_out_wins[i].size);

	/*
	 * For config space accesses on the RC, show the right class for
	 * a PCIe-PCIe bridge (the default setting is to be EP mode).
	 */
	WR_FLD_RB(m_base, PCIE_RC_CFG_PRIV1_ID_VAL3, CLASS_CODE, 0x060400);

	u16 lnksta = bcm_readw(m_base + BRCM_PCIE_CAP_REGS + PCI_EXP_LNKSTA);
	u16 cls = lnksta & PCI_EXP_LNKSTA_CLS;
	u16 nlw = (lnksta & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;

	CLogger::Get ()->Write (FromPCIeHost, LogNotice, "Link up, %s Gbps x%u",
				link_speed_to_str(cls), nlw);
#endif

#if RASPPI == 4
	/* PCIe->SCB endian mode for BAR */
	/* field ENDIAN_MODE_BAR2 = DATA_ENDIAN */
	WR_FLD_RB(m_base, PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1, ENDIAN_MODE_BAR2, DATA_ENDIAN);

	/*
	 * Refclk from RC should be gated with CLKREQ# input when ASPM L0s,L1
	 * is enabled =>  setting the CLKREQ_DEBUG_ENABLE field to 1.
	 */
	WR_FLD_RB(m_base, PCIE_MISC_HARD_PCIE_HARD_DEBUG, CLKREQ_DEBUG_ENABLE, 1);
#else
	pcie_config_clkreq();
#endif

	return 0;
}

int CBcmPCIeHostBridge::enable_bridge (void)
{
	uintptr conf = pcie_map_conf (PCI_BUS (0), PCI_DEVFN(0, 0), 0);
	if (!conf)
		return -1;

	if (   read32 (conf + PCI_CLASS_REVISION) >> 8 != 0x060400
	    || read8 (conf + PCI_HEADER_TYPE) != PCI_HEADER_TYPE_BRIDGE)
		return -1;

	write8 (conf + PCI_CACHE_LINE_SIZE, 64/4);	// TODO: get this from cache config

	write8 (conf + PCI_SECONDARY_BUS, PCI_BUS (1));
	write8 (conf + PCI_SUBORDINATE_BUS, PCI_BUS (1));

	write16 (conf + PCI_MEMORY_BASE, m_out_wins[0].pcie_addr >> 16);
	write16 (conf + PCI_MEMORY_LIMIT, m_out_wins[0].pcie_addr >> 16);

	write8 (conf + PCI_BRIDGE_CONTROL, PCI_BRIDGE_CTL_PARITY);

	assert (read8 (conf + BRCM_PCIE_CAP_REGS + PCI_CAP_LIST_ID) == PCI_CAP_ID_EXP);
	write8 (conf + BRCM_PCIE_CAP_REGS + PCI_EXP_RTCTL, PCI_EXP_RTCTL_CRSSVE);

	write16 (conf + PCI_COMMAND,   PCI_COMMAND_MEMORY
				     | PCI_COMMAND_MASTER
				     | PCI_COMMAND_PARITY
				     | PCI_COMMAND_SERR);

	return 0;
}

int CBcmPCIeHostBridge::enable_device (u32 nClassCode, unsigned nSlot, unsigned nFunc)
{
	assert (nClassCode >> 24 == 0);
	assert (nSlot < 32);
	assert (nFunc < 8);

	uintptr conf = pcie_map_conf (PCI_BUS (1), PCI_DEVFN (nSlot, nFunc), 0);
	if (!conf)
		return -1;

	if (   read32 (conf + PCI_CLASS_REVISION) >> 8 != nClassCode
	    || read8 (conf + PCI_HEADER_TYPE) != PCI_HEADER_TYPE_NORMAL)
	{
		CLogger::Get ()->Write (FromPCIeHost, LogError, "Invalid class code (0x%06X)",
					read32 (conf + PCI_CLASS_REVISION) >> 8);

		return -1;
	}

	write8 (conf + PCI_CACHE_LINE_SIZE, 64/4);	// TODO: get this from cache config

	write32 (conf + PCI_BASE_ADDRESS_0,   lower_32_bits (m_out_wins[0].pcie_addr)
					    | PCI_BASE_ADDRESS_MEM_TYPE_64);
	write32 (conf + PCI_BASE_ADDRESS_1, upper_32_bits (m_out_wins[0].pcie_addr));

#if 0
	uintptr msi_conf = find_pci_capability (conf, PCI_CAP_ID_MSI);
	if (!msi_conf)
		return -1;

	write32 (msi_conf + PCI_MSI_ADDRESS_LO, lower_32_bits (m_msi_target_addr));
	write32 (msi_conf + PCI_MSI_ADDRESS_HI, upper_32_bits (m_msi_target_addr));
	write16 (msi_conf + PCI_MSI_DATA_64, 0x6540);

	u8 uchQueueSize = (read8 (msi_conf + PCI_MSI_FLAGS) & PCI_MSI_FLAGS_QMASK) >> 1;
	write8 (msi_conf + PCI_MSI_FLAGS,   PCI_MSI_FLAGS_ENABLE
					  | PCI_MSI_FLAGS_64BIT
					  | uchQueueSize << 4);
#else
	// Ensure, that we can use INTA.
	u8 uchIntPin = read8 (conf + PCI_INTERRUPT_PIN);
	if (uchIntPin != 1)
	{
		CLogger::Get ()->Write (FromPCIeHost, LogWarning, "INTA was not enabled (%u)",
					(unsigned) uchIntPin);

		write8 (conf + PCI_INTERRUPT_PIN, 1);
	}
#endif

	write16 (conf + PCI_COMMAND,   PCI_COMMAND_MEMORY
				     | PCI_COMMAND_MASTER
				     | PCI_COMMAND_PARITY
				     | PCI_COMMAND_SERR
				     //| PCI_COMMAND_INTX_DISABLE
		);

	return 0;
}

int CBcmPCIeHostBridge::pcie_set_pci_ranges(void)
{
	assert (m_num_out_wins == 0);

	TMemoryWindow Memory = CMachineInfo::Get ()->GetPCIeMemory (m_nBus);

	m_out_wins[0].cpu_addr = Memory.CPUAddress;
	m_out_wins[0].pcie_addr = Memory.BusAddress;
	m_out_wins[0].size = Memory.Size;

	m_num_out_wins = 1;

	return 0;
}

int CBcmPCIeHostBridge::pcie_set_dma_ranges(void)
{
	assert (m_num_dma_ranges == 0);

	TMemoryWindow DMAMemory = CMachineInfo::Get ()->GetPCIeDMAMemory (m_nBus);

	m_dma_ranges[0].pcie_addr = DMAMemory.BusAddress;
	m_dma_ranges[0].cpu_addr = DMAMemory.CPUAddress;
	m_dma_ranges[0].size = DMAMemory.Size;

	m_num_dma_ranges = 1;

	s_nDMAAddress[m_nBus] = DMAMemory.BusAddress;

	return 0;
}

void CBcmPCIeHostBridge::pcie_set_outbound_win(unsigned win, u64 cpu_addr,
					       u64 pcie_addr, u64 size)
{
	u64 cpu_addr_mb, limit_addr_mb;
	u32 tmp;

	/* Set the m_base of the pcie_addr window */
	bcm_writel(lower_32_bits(pcie_addr) + MMIO_ENDIAN,
		   m_base + PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LO + (win * 8));
	bcm_writel(upper_32_bits(pcie_addr),
		   m_base + PCIE_MISC_CPU_2_PCIE_MEM_WIN0_HI + (win * 8));

	cpu_addr_mb = cpu_addr >> 20;
	limit_addr_mb = (cpu_addr + size - 1) >> 20;

	/* Write the addr m_base low register */
	WR_FLD_WITH_OFFSET(m_base, (win * 4),
			   PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT,
			   BASE, cpu_addr_mb);
	/* Write the addr limit low register */
	WR_FLD_WITH_OFFSET(m_base, (win * 4),
			   PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT,
			   LIMIT, limit_addr_mb);

	/* Write the cpu addr high register */
	tmp = (u32)(cpu_addr_mb >> PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_NUM_MASK_BITS);
	WR_FLD_WITH_OFFSET(m_base, (win * 8),
			   PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_HI,
			   BASE, tmp);
	/* Write the cpu limit high register */
	tmp = (u32)(limit_addr_mb >>
		PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT_NUM_MASK_BITS);
	WR_FLD_WITH_OFFSET(m_base, (win * 8),
			   PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LIMIT_HI,
			   LIMIT, tmp);
}

uintptr CBcmPCIeHostBridge::pcie_map_conf(unsigned busnr, unsigned devfn, int where)
{
	/* Accesses to the RC go right to the RC registers if slot==0 */
	if (busnr == 0)
		return PCI_SLOT(devfn) ? 0 : m_base + where;

	/* For devices, write to the config space index register */
	int idx = cfg_index(busnr, devfn, 0);
	bcm_writel(idx, m_base + PCIE_EXT_CFG_INDEX);
	return m_base + PCIE_EXT_CFG_DATA + where;
}


uintptr CBcmPCIeHostBridge::find_pci_capability (uintptr nPCIConfig, u8 uchCapID)
{
	assert (nPCIConfig != 0);
	assert (uchCapID > 0);

	if (!(read16 (nPCIConfig + PCI_STATUS) & PCI_STATUS_CAP_LIST))
	{
		return 0;
	}

	u8 uchCapPtr = read8 (nPCIConfig + PCI_CAPABILITY_LIST);
	while (uchCapPtr != 0)
	{
		if (read8 (nPCIConfig + uchCapPtr + PCI_CAP_LIST_ID) == uchCapID)
		{
			return nPCIConfig + uchCapPtr;
		}

		uchCapPtr = read8 (nPCIConfig + uchCapPtr + PCI_CAP_LIST_NEXT);
	}

	return 0;
}

void CBcmPCIeHostBridge::pcie_bridge_sw_init_set(unsigned val)
{
#if RASPPI == 4
	unsigned shift = RGR1_SW_INIT_1_INIT_GENERIC_SHIFT;
	u32 mask =  RGR1_SW_INIT_1_INIT_GENERIC_MASK;

	wr_fld_rb(m_base + PCIE_RGR1_SW_INIT_1, mask, shift, val);
#else
	if (val)
		reset_assert(m_nBus == PCIE_BUS_ONBOARD ? 44 : 43);
	else
		reset_deassert(m_nBus == PCIE_BUS_ONBOARD ? 44 : 43);
#endif
}

void CBcmPCIeHostBridge::pcie_perst_set(unsigned int val)
{
#if RASPPI == 4
	wr_fld_rb(m_base + PCIE_RGR1_SW_INIT_1, PCIE_RGR1_SW_INIT_1_PERST_MASK,
		  PCIE_RGR1_SW_INIT_1_PERST_SHIFT, val);
#else
	/* Perst bit has moved and assert value is 0 */
	u32 tmp = bcm_readl(m_base + PCIE_MISC_PCIE_CTRL);
	tmp = INSERT_FIELD(tmp, PCIE_MISC_PCIE_CTRL, PCIE_PERSTB, !val);
	bcm_writel(tmp, m_base +  PCIE_MISC_PCIE_CTRL);
#endif
}

bool CBcmPCIeHostBridge::pcie_link_up(void)
{
	u32 val = bcm_readl(m_base + PCIE_MISC_PCIE_STATUS);
	u32 dla = EXTRACT_FIELD(val, PCIE_MISC_PCIE_STATUS, PCIE_DL_ACTIVE);
	u32 plu = EXTRACT_FIELD(val, PCIE_MISC_PCIE_STATUS, PCIE_PHYLINKUP);

	return  (dla && plu) ? true : false;
}

/* The controller is capable of serving in both RC and EP roles */
bool CBcmPCIeHostBridge::pcie_rc_mode(void)
{
#if RASPPI == 4
	u32 val = bcm_readl(m_base + PCIE_MISC_PCIE_STATUS);

	return !!EXTRACT_FIELD(val, PCIE_MISC_PCIE_STATUS, PCIE_PORT);
#else
	return true;	// TODO
#endif
}

int CBcmPCIeHostBridge::pcie_enable_msi(TPCIeMSIHandler *pHandler, void *pParam)
{
	assert (pHandler != 0);

	m_msi = new TPCIeMSIData;
	if (!m_msi)
		return -1;
	memset (m_msi, 0, sizeof *m_msi);

	m_msi->base = m_base;
	m_msi->rev = m_rev;
	m_msi->target_addr = m_msi_target_addr;
	m_msi->handler = pHandler;
	m_msi->param = pParam;

	assert (m_pInterrupt != 0);
	m_pInterrupt->ConnectIRQ (ARM_IRQ_PCIE_HOST_MSI, InterruptHandler, m_msi);

	assert (m_rev >= BRCM_PCIE_HW_REV_33);
	m_msi->intr_base = m_msi->base + PCIE_MSI_INTR2_BASE;

	msi_set_regs(m_msi);

	return 0;
}

void CBcmPCIeHostBridge::msi_set_regs(TPCIeMSIData *msi)
{
	assert (msi->rev >= BRCM_PCIE_HW_REV_33);
	/*
	 * ffe0 -- least sig 5 bits are 0 indicating 32 msgs
	 * 6540 -- this is our arbitrary unique data value
	 */
	u32 data_val = 0xffe06540;

	/*
	 * Make sure we are not masking MSIs. Note that MSIs can be masked,
	 * but that occurs on the PCIe EP device
	 */
	bcm_writel(0xffffffff, msi->intr_base + MASK_CLR);

	u32 msi_lo = lower_32_bits(msi->target_addr);
	u32 msi_hi = upper_32_bits(msi->target_addr);
	/*
	 * The 0 bit of PCIE_MISC_MSI_BAR_CONFIG_LO is repurposed to MSI
	 * enable, which we set to 1.
	 */
	bcm_writel(msi_lo | 1, msi->base + PCIE_MISC_MSI_BAR_CONFIG_LO);
	bcm_writel(msi_hi, msi->base + PCIE_MISC_MSI_BAR_CONFIG_HI);
	bcm_writel(data_val, msi->base + PCIE_MISC_MSI_DATA_CONFIG);
}

/* Configuration space read/write support */
int CBcmPCIeHostBridge::cfg_index(int busnr, int devfn, int reg)
{
	return    ((PCI_SLOT(devfn) & 0x1f) << PCIE_SLOT_SHIFT)
		| ((PCI_FUNC(devfn) & 0x07) << PCIE_FUNC_SHIFT)
		| (busnr << PCIE_BUSNUM_SHIFT)
		| (reg & ~3);
}

/* Limits operation to a specific generation (1, 2, or 3) */
void CBcmPCIeHostBridge::set_gen(uintptr base, int gen)
{
	u32 lnkcap = bcm_readl(base + BRCM_PCIE_CAP_REGS + PCI_EXP_LNKCAP);
	u16 lnkctl2 = bcm_readw(base + BRCM_PCIE_CAP_REGS + PCI_EXP_LNKCTL2);

	lnkcap = (lnkcap & ~PCI_EXP_LNKCAP_SLS) | gen;
	bcm_writel(lnkcap, base + BRCM_PCIE_CAP_REGS + PCI_EXP_LNKCAP);

	lnkctl2 = (lnkctl2 & ~0xf) | gen;
	bcm_writew(lnkctl2, base + BRCM_PCIE_CAP_REGS + PCI_EXP_LNKCTL2);
}

const char *CBcmPCIeHostBridge::link_speed_to_str(int s)
{
	switch (s) {
	case 1:
		return "2.5";
	case 2:
		return "5.0";
	case 3:
		return "8.0";
	default:
		break;
	}
	return "???";
}

/*
 * This is to convert the size of the inbound "BAR" region to the
 * non-linear values of PCIE_X_MISC_RC_BAR[123]_CONFIG_LO.SIZE
 */
int CBcmPCIeHostBridge::encode_ibar_size(u64 size)
{
	int log2_in = ilog2(size);

	if (log2_in >= 12 && log2_in <= 15)
		/* Covers 4KB to 32KB (inclusive) */
		return (log2_in - 12) + 0x1c;
	else if (log2_in >= 16 && log2_in <= 37)
		/* Covers 64KB to 32GB, (inclusive) */
		return log2_in - 15;
	/* Something is awry so disable */
	return 0;
}

u32 CBcmPCIeHostBridge::rd_fld(uintptr p, u32 mask, int shift)
{
	return (bcm_readl(p) & mask) >> shift;
}

void CBcmPCIeHostBridge::wr_fld(uintptr p, u32 mask, int shift, u32 val)
{
	u32 reg = bcm_readl(p);

	reg = (reg & ~mask) | ((val << shift) & mask);
	bcm_writel(reg, p);
}

void CBcmPCIeHostBridge::wr_fld_rb(uintptr p, u32 mask, int shift, u32 val)
{
	wr_fld(p, mask, shift, val);
	(void)bcm_readl(p);
}

void CBcmPCIeHostBridge::InterruptHandler (void *pParam)
{
	TPCIeMSIData *msi = (TPCIeMSIData *) pParam;
	assert (msi != 0);

	u32 status;
	while ((status = bcm_readl(msi->intr_base + STATUS)) != 0)
	{
		for (unsigned vector = 0; status && vector < BRCM_INT_PCI_MSI_NR; vector++)
		{
			u32 mask = 1 << vector;

			if (!(status & mask))
			{
				continue;
			}

			/* clear the interrupt */
			bcm_writel(mask, msi->intr_base + CLR);

			assert (msi->handler != 0);
			(*msi->handler) (vector, msi->param);

			status &= ~mask;
		}
	}

	bcm_writel(1, msi->base + PCIE_MISC_EOI_CTRL);
}

void CBcmPCIeHostBridge::usleep_range (unsigned min, unsigned max)
{
	CTimer::SimpleusDelay (min);
}

void CBcmPCIeHostBridge::msleep (unsigned ms)
{
	CTimer::SimpleMsDelay (ms);
}

int CBcmPCIeHostBridge::ilog2 (u64 v)
{
	int l = 0;
	while (((u64) 1 << l) < v)
		l++;
	return l;
}

#if RASPPI >= 5

void CBcmPCIeHostBridge::pcie_set_tc_qos(void)
{
	u32 reg;

	/* XXX: BCM2712C0 is broken, disable the forwarding search */
	reg = bcm_readl(m_base + PCIE_MISC_AXI_INTF_CTRL);
	reg &= ~AXI_REQFIFO_EN_QOS_PROPAGATION;
	bcm_writel(reg, m_base + PCIE_MISC_AXI_INTF_CTRL);

	/* Disable VDM reception by default - QoS map defaults to 0 */
	reg = bcm_readl(m_base + PCIE_MISC_CTRL_1);
	reg &= ~PCIE_MISC_CTRL_1_EN_VDM_QOS_CONTROL_MASK;
	bcm_writel(reg, m_base + PCIE_MISC_CTRL_1);

	reg = bcm_readl(m_base + PCIE_MISC_CTRL_1);
	reg |= PCIE_MISC_CTRL_1_EN_VDM_QOS_CONTROL_MASK;
	bcm_writel(reg, m_base + PCIE_MISC_CTRL_1);

	/* No forwarding means no point separating panic priorities from normal */
	u32 qos_map = 0xBBAA9888U;
	bcm_writel(qos_map, m_base + PCIE_MISC_VDM_PRIORITY_TO_QOS_MAP_LO);
	bcm_writel(qos_map, m_base + PCIE_MISC_VDM_PRIORITY_TO_QOS_MAP_HI);

	/* Match Vendor ID of 0 */
	bcm_writel(0, m_base + PCIE_RC_TL_VDM_CTL1);
	/* Forward VDMs to priority interface - at least the rx counters work */
	reg = bcm_readl(m_base + PCIE_RC_TL_VDM_CTL0);
	reg |= PCIE_RC_TL_VDM_CTL0_VDM_ENABLED_MASK |
	       PCIE_RC_TL_VDM_CTL0_VDM_IGNORETAG_MASK |
	       PCIE_RC_TL_VDM_CTL0_VDM_IGNOREVNDRID_MASK;
	bcm_writel(reg, m_base + PCIE_RC_TL_VDM_CTL0);
}

// TODO: Requires to read PCIe capabilities, but seems to work without.
void CBcmPCIeHostBridge::pcie_config_clkreq(void)
{
	u32 tmp = bcm_readl(m_base + PCIE_MISC_HARD_PCIE_HARD_DEBUG);
	tmp &= ~PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_DEBUG_ENABLE_MASK;
	tmp &= ~PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_L1SS_ENABLE_MASK;
	bcm_writel(tmp, m_base + PCIE_MISC_HARD_PCIE_HARD_DEBUG);
}

void CBcmPCIeHostBridge::pcie_munge_pll(void)
{
	unsigned i;
	u8 regs[] =  { 0x16,   0x17,   0x18,   0x19,   0x1b,   0x1c,   0x1e };
	u16 data[] = { 0x50b9, 0xbda1, 0x0094, 0x97b4, 0x5030, 0x5030, 0x0007 };

	pcie_mdio_write(MDIO_PORT0, SET_ADDR_OFFSET, 0x1600);

	for (i = 0; i < sizeof regs / sizeof regs[0]; i++) {
		pcie_mdio_write(MDIO_PORT0, regs[i], data[i]);
	}

	usleep_range(100, 200);
}

int CBcmPCIeHostBridge::pcie_mdio_read(u8 port, u8 regad, u32 *val)
{
	int tries;
	u32 data;

	bcm_writel(pcie_mdio_form_pkt(port, regad, MDIO_CMD_READ),
		   m_base + PCIE_RC_DL_MDIO_ADDR);
	bcm_readl(m_base + PCIE_RC_DL_MDIO_ADDR);

	data = bcm_readl(m_base + PCIE_RC_DL_MDIO_RD_DATA);
	for (tries = 0; !MDIO_RD_DONE(data) && tries < 10; tries++) {
		usleep_range (10, 10);
		data = bcm_readl(m_base + PCIE_RC_DL_MDIO_RD_DATA);
	}

	*val = (data & MDIO_DATA_MASK) >> MDIO_DATA_SHIFT;
	return MDIO_RD_DONE(data) ? 0 : -1;
}

int CBcmPCIeHostBridge::pcie_mdio_write(u8 port, u8 regad, u16 wrdata)
{
	int tries;
	u32 data;

	bcm_writel(pcie_mdio_form_pkt(port, regad, MDIO_CMD_WRITE),
		   m_base + PCIE_RC_DL_MDIO_ADDR);
	bcm_readl(m_base + PCIE_RC_DL_MDIO_ADDR);
	bcm_writel(MDIO_DATA_DONE_MASK | wrdata, m_base + PCIE_RC_DL_MDIO_WR_DATA);

	data = bcm_readl(m_base + PCIE_RC_DL_MDIO_WR_DATA);
	for (tries = 0; !MDIO_WT_DONE(data) && tries < 10; tries++) {
		usleep_range (10, 10);
		data = bcm_readl(m_base + PCIE_RC_DL_MDIO_WR_DATA);
	}

	return MDIO_WT_DONE(data) ? 0 : -1;
}

u32 CBcmPCIeHostBridge::pcie_mdio_form_pkt(int port, int regad, int cmd)
{
	u32 pkt = port << MDIO_PORT_SHIFT;
	pkt |= regad << MDIO_REGAD_SHIFT;
	pkt |= cmd << MDIO_CMD_SHIFT;

	return pkt;
}

// This has been ported from the Linux driver which is:
//	drivers/reset/reset-brcmstb.c
//	Author: Florian Fainelli <f.fainelli@gmail.com>
//	Copyright (C) 2018 Broadcom
//	Licensed under GPL-2.0

#define SW_INIT_SET		0x00
#define SW_INIT_CLEAR		0x04
#define SW_INIT_STATUS		0x08

#define SW_INIT_BIT(id)		BIT((id) & 0x1f)
#define SW_INIT_BANK(id)	((id) >> 5)

/* A full bank contains extra registers that we are not utilizing but still
 * qualify as a single bank.
 */
#define SW_INIT_BANK_SIZE	0x18

int CBcmPCIeHostBridge::reset_assert(unsigned long id)
{
	unsigned int off = SW_INIT_BANK(id) * SW_INIT_BANK_SIZE;

	bcm_writel(SW_INIT_BIT(id), ARM_RESET_BASE + off + SW_INIT_SET);

	return 0;
}

int CBcmPCIeHostBridge::reset_deassert(unsigned long id)
{
	unsigned int off = SW_INIT_BANK(id) * SW_INIT_BANK_SIZE;

	bcm_writel(SW_INIT_BIT(id), ARM_RESET_BASE + off + SW_INIT_CLEAR);
	/* Maximum reset delay after de-asserting a line and seeing block
	 * operation is typically 14us for the worst case, build some slack
	 * here.
	 */
	usleep_range(100, 200);

	return 0;
}

// This has been ported from the Linux driver which is:
//	drivers/reset/reset-brcmstb-rescal.c
//	Copyright (C) 2018-2020 Broadcom
//	Licensed under GPL-2.0

#define BRCM_RESCAL_START	0x0
#define  BRCM_RESCAL_START_BIT	BIT(0)
#define BRCM_RESCAL_CTRL	0x4
#define BRCM_RESCAL_STATUS	0x8
#define  BRCM_RESCAL_STATUS_BIT	BIT(0)

/* Also doubles a deassert */
int CBcmPCIeHostBridge::rescal_reset_deassert(void)
{
	u32 reg = bcm_readl(ARM_RESET_RESCAL_BASE + BRCM_RESCAL_START);
	bcm_writel(reg | BRCM_RESCAL_START_BIT, ARM_RESET_RESCAL_BASE + BRCM_RESCAL_START);
	reg = bcm_readl(ARM_RESET_RESCAL_BASE + BRCM_RESCAL_START);
	if (!(reg & BRCM_RESCAL_START_BIT)) {
		CLogger::Get ()->Write (FromPCIeHost, LogError, "Failed to start SATA/PCIe rescal");
		return -1;
	}

	unsigned nStartTicks = CTimer::GetClockTicks ();
	while (!(bcm_readl(ARM_RESET_RESCAL_BASE + BRCM_RESCAL_STATUS) & BRCM_RESCAL_STATUS_BIT)) {
		if (CTimer::GetClockTicks () - nStartTicks >= CLOCKHZ/1000) {
			CLogger::Get ()->Write (FromPCIeHost, LogError, "Time out on SATA/PCIe rescal");
			return -1;
		}

		usleep_range(25, 100);
	}

	reg = bcm_readl(ARM_RESET_RESCAL_BASE + BRCM_RESCAL_START);
	bcm_writel(reg & ~BRCM_RESCAL_START_BIT, ARM_RESET_RESCAL_BASE + BRCM_RESCAL_START);

	return 0;
}

#endif
