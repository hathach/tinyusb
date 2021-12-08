# DWC2 Hardware Configuration Registers

## Broadcom BCM2711 (Pi4)

dwc2->guid = 2708A000
dwc2->gsnpsid = 4F54280A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 228DDD50
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 1
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 7
hw_cfg2->num_host_ch = 7
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 0
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = FF000E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 4080

dwc2->ghwcfg4 = 1FF00020
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 0
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 0
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 15
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## EFM32GG FS

dwc2->guid = 0
dwc2->gsnpsid = 4F54330A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 228F5910
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 6
hw_cfg2->num_host_ch = 13
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 0
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 1F204E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 1
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 498

dwc2->ghwcfg4 = 1BF08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 13
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## ESP32-S2 Fullspeed

dwc2->guid = 0
dwc2->gsnpsid = 4F54400A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 224DD930
hw_cfg2->op_mode = 2
hw_cfg2->arch = 3
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 1
hw_cfg2->fs_phy_type = 2
hw_cfg2->num_dev_ep = 6
hw_cfg2->num_host_ch = 9
hw_cfg2->period_channel_support = 0
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 1
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 22
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = C804B5
hw_cfg3->xfer_size_width = 10
hw_cfg3->packet_size_width = 5
hw_cfg3->otg_enable = 0
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 1
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 1
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 23130

dwc2->ghwcfg4 = D3F0A030
hw_cfg4->num_dev_period_in_ep = 10
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 0
hw_cfg4->hibernation = 1
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 1
hw_cfg4->acg_enable = 1
hw_cfg4->utmi_phy_data_width = 1
hw_cfg4->dev_ctrl_ep_num = 10
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 0
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 0
hw_cfg4->dedicated_fifos = 0
hw_cfg4->num_dev_in_eps = 13
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 1

## STM32F407 and STM32F207

STM32F407 and STM32F207 are exactly the same

### STM32F407 Fullspeed

dwc2->guid = 1200
dwc2->gsnpsid = 4F54281A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229DCD20
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 3
hw_cfg2->num_host_ch = 7
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 20001E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = FF08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 7
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

### STM32F407 Highspeed

dwc2->guid = 1100
dwc2->gsnpsid = 4F54281A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229ED590
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 2
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 5
hw_cfg2->num_host_ch = 11
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 3F403E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 1
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 1012

dwc2->ghwcfg4 = 17F00030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 0
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 11
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## STM32F411 Fullspeed

dwc2->guid = 1200
dwc2->gsnpsid = 4F54281A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229DCD20
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 3
hw_cfg2->num_host_ch = 7
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 20001E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = FF08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 7
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## STM32F412 FS

dwc2->guid = 2000
dwc2->gsnpsid = 4F54320A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229ED520
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 5
hw_cfg2->num_host_ch = 11
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 200D1E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = 17F08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 11
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## STM32F723

### STM32F723 HighSpeed

dwc2->guid = 3100
dwc2->gsnpsid = 4F54330A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229FE1D0
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 3
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 8
hw_cfg2->num_host_ch = 15
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 3EED2E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 1
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 1006

dwc2->ghwcfg4 = 23F00030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 0
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 1
hw_cfg4->dma_desc_enable = 1
hw_cfg4->dma_dynamic = 0

### STM32F723 Fullspeed

dwc2->guid = 3000
dwc2->gsnpsid = 4F54330A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229ED520
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 5
hw_cfg2->num_host_ch = 11
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 200D1E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = 17F08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 11
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## STM32F767 FS

dwc2->guid = 2000
dwc2->gsnpsid = 4F54320A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229ED520
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 5
hw_cfg2->num_host_ch = 11
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 200D1E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = 17F08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 11
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## STM32H743 (both cores HS)

dwc2->guid = 2300
dwc2->gsnpsid = 4F54330A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229FE190
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 2
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 8
hw_cfg2->num_host_ch = 15
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 3B8D2E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 1
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 952

dwc2->ghwcfg4 = E3F00030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 0
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 1
hw_cfg4->dma_desc_enable = 1
hw_cfg4->dma_dynamic = 1

## STM32L476 FS

dwc2->guid = 2000
dwc2->gsnpsid = 4F54310A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 229ED520
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 5
hw_cfg2->num_host_ch = 11
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 1
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 200D1E8
hw_cfg3->xfer_size_width = 8
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 1
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 1
hw_cfg3->lpm_mode = 1
hw_cfg3->total_fifo_size = 512

dwc2->ghwcfg4 = 17F08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 11
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## GD32VF103 Fullspeed

dwc2->guid = 1000
dwc2->gsnpsid = 0
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 0
hw_cfg2->op_mode = 0
hw_cfg2->arch = 0
hw_cfg2->point2point = 0
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 0
hw_cfg2->num_dev_ep = 0
hw_cfg2->num_host_ch = 0
hw_cfg2->period_channel_support = 0
hw_cfg2->enable_dynamic_fifo = 0
hw_cfg2->mul_cpu_int = 0
hw_cfg2->nperiod_tx_q_depth = 0
hw_cfg2->host_period_tx_q_depth = 0
hw_cfg2->dev_token_q_depth = 0
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 0
hw_cfg3->xfer_size_width = 0
hw_cfg3->packet_size_width = 0
hw_cfg3->otg_enable = 0
hw_cfg3->i2c_enable = 0
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 0

dwc2->ghwcfg4 = 0
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 0
hw_cfg4->ahb_freq_min = 0
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 0
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 0
hw_cfg4->vbus_valid_filter_enabled = 0
hw_cfg4->a_valid_filter_enabled = 0
hw_cfg4->b_valid_filter_enabled = 0
hw_cfg4->dedicated_fifos = 0
hw_cfg4->num_dev_in_eps = 0
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 0

## XMC4500

dwc2->guid = AEC000
dwc2->gsnpsid = 4F54292A
dwc2->ghwcfg1 = 0

dwc2->ghwcfg2 = 228F5930
hw_cfg2->op_mode = 0
hw_cfg2->arch = 2
hw_cfg2->point2point = 1
hw_cfg2->hs_phy_type = 0
hw_cfg2->fs_phy_type = 1
hw_cfg2->num_dev_ep = 6
hw_cfg2->num_host_ch = 13
hw_cfg2->period_channel_support = 1
hw_cfg2->enable_dynamic_fifo = 1
hw_cfg2->mul_cpu_int = 0
hw_cfg2->nperiod_tx_q_depth = 2
hw_cfg2->host_period_tx_q_depth = 2
hw_cfg2->dev_token_q_depth = 8
hw_cfg2->otg_enable_ic_usb = 0

dwc2->ghwcfg3 = 27A01E5
hw_cfg3->xfer_size_width = 5
hw_cfg3->packet_size_width = 6
hw_cfg3->otg_enable = 1
hw_cfg3->i2c_enable = 1
hw_cfg3->vendor_ctrl_itf = 0
hw_cfg3->optional_feature_removed = 0
hw_cfg3->synch_reset = 0
hw_cfg3->otg_adp_support = 0
hw_cfg3->otg_enable_hsic = 0
hw_cfg3->battery_charger_support = 0
hw_cfg3->lpm_mode = 0
hw_cfg3->total_fifo_size = 634

dwc2->ghwcfg4 = DBF08030
hw_cfg4->num_dev_period_in_ep = 0
hw_cfg4->power_optimized = 1
hw_cfg4->ahb_freq_min = 1
hw_cfg4->hibernation = 0
hw_cfg4->service_interval_mode = 0
hw_cfg4->ipg_isoc_en = 0
hw_cfg4->acg_enable = 0
hw_cfg4->utmi_phy_data_width = 2
hw_cfg4->dev_ctrl_ep_num = 0
hw_cfg4->iddg_filter_enabled = 1
hw_cfg4->vbus_valid_filter_enabled = 1
hw_cfg4->a_valid_filter_enabled = 1
hw_cfg4->b_valid_filter_enabled = 1
hw_cfg4->dedicated_fifos = 1
hw_cfg4->num_dev_in_eps = 13
hw_cfg4->dma_desc_enable = 0
hw_cfg4->dma_dynamic = 1
