#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#define BUTTON_GPIO GPIO_NUM_14

// Receiver's custom STA MAC (set this to match the receiver board)
uint8_t receiver_mac[] = {0x02, 0x00, 0x00, 0xAA, 0xBB, 0x02};

// Track current state: 0 = disarmed, 1 = armed/panic
int panic_state = 0;

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

    // Configure button
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLDOWN_ONLY);

    while (1) {
        if (gpio_get_level(BUTTON_GPIO)) {
            if (panic_state == 0) {
                char msg[] = "PANIC";
                esp_now_send(receiver_mac, (uint8_t *)msg, strlen(msg));
                panic_state = 1; // now armed
                printf("Sent PANIC\n");
            } else {
                char msg[] = "DISARM";
                esp_now_send(receiver_mac, (uint8_t *)msg, strlen(msg));
                panic_state = 0; // now disarmed
                printf("Sent DISARM\n");
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // poll delay
    }
}