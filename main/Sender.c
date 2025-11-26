#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#define LOCK_BUTTON_GPIO   GPIO_NUM_12   // Lock/Arm button
#define UNLOCK_BUTTON_GPIO GPIO_NUM_13   // Unlock/Disarm button
#define PANIC_BUTTON_GPIO  GPIO_NUM_14   // Panic button

// Receiver's custom STA MAC (set this to match the receiver board)
uint8_t receiver_mac[] = {0x02, 0x00, 0x00, 0xAA, 0xBB, 0x02};

void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    printf("Send status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void app_main(void) {
    // Init NVS + Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set custom MAC for sender
    uint8_t custom_sta_mac[6] = {0x02, 0x00, 0x00, 0xAA, 0xBB, 0x01};
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, custom_sta_mac);
    if (err != ESP_OK) {
        printf("Failed to set MAC, error: %d\n", err);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Print back MAC
    uint8_t mac_sta[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac_sta));
    printf("Sender STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac_sta[0], mac_sta[1], mac_sta[2], mac_sta[3], mac_sta[4], mac_sta[5]);

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(send_cb);

    // Add peer (receiver)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    // Configure buttons
    gpio_set_direction(LOCK_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LOCK_BUTTON_GPIO, GPIO_PULLDOWN_ONLY);

    gpio_set_direction(UNLOCK_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(UNLOCK_BUTTON_GPIO, GPIO_PULLDOWN_ONLY);

    gpio_set_direction(PANIC_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PANIC_BUTTON_GPIO, GPIO_PULLDOWN_ONLY);

    while (1) {
        if (gpio_get_level(LOCK_BUTTON_GPIO)) {
            char msg[] = "ARM";
            esp_now_send(receiver_mac, (uint8_t *)msg, strlen(msg));
            printf("Sent ARM (lock)\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // debounce
        }

        if (gpio_get_level(UNLOCK_BUTTON_GPIO)) {
            char msg[] = "DISARM";
            esp_now_send(receiver_mac, (uint8_t *)msg, strlen(msg));
            printf("Sent DISARM (unlock)\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // debounce
        }

        if (gpio_get_level(PANIC_BUTTON_GPIO)) {
            char msg[] = "PANIC";
            esp_now_send(receiver_mac, (uint8_t *)msg, strlen(msg));
            printf("Sent PANIC\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // debounce
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // poll delay
    }
}