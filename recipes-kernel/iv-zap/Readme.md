
/ {
    reserved-memory {                
        #address-cells = <0x02>;
        #size-cells = <0x02>;
        ranges;
                                                         
        iv_zap_pool:iv_zap_pool@0xee00000 {                  
            reg = <0x00 0xee00000 0x00 0x8000000>;
        };                                   
    };      
};      

&{/amba} {
    iv_zap {
        compatible = "iv,zap";
        memory-region = <0x1a>;
        num-devices = <0x10>;
        reg = <0x00 0x98000000 0x00 0x1000>;
        irq = <0x69>;
        pool-sizes = <0x00 0x8000000 0x00 0x8000000>;
    };
}
