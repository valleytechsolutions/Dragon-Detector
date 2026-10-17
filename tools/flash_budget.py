"""Report flash allocations and enforce the application-slot limit."""
RESERVE_PERCENT=5
def report(entries,app_bytes,flash_bytes):
    app_size=int(entries['app0'][4],0)
    reserve=(app_size*RESERVE_PERCENT+99)//100
    assert app_bytes<=app_size-reserve, f'App exceeds {100-RESERVE_PERCENT}% of OTA slot: {app_bytes}/{app_size}'
    return {'reserve_percent':RESERVE_PERCENT,'app_bytes':app_bytes,'app_slot_bytes':app_size,'app_slot_free_bytes':app_size-app_bytes,'app_slot_free_percent':round(100*(app_size-app_bytes)/app_size,2),'physical_flash_bytes':flash_bytes,'ota_slot_bytes':int(entries['app1'][4],0),'filesystem_partitions':[{ 'name':name,'offset':int(row[3],0),'bytes':int(row[4],0)} for name,row in entries.items() if row[2].strip() in ('spiffs','littlefs')],'note':'OTA and filesystem allocations are not occupied application bytes. Existing user-data usage is device-specific.'}
