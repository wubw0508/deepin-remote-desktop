## lightdm local seat启动流程

1. seat_new
2. seat_set_name
3. display_manager_add_seat
4. seat_start
5. seat_local_setup
6. seat_local_start
7. seat_real_start
8. create_greeter_session 不是logind意义的session
9. seat_local_create_greeter_session
10. seat_real_create_greeter_session
11. create_display_server
12. display_server_ready_cb
13. seat_local_run_script
14. start_session
15. session_start
16. session_real_start
17. fork child
18. session_child_run 进行pam流程
19. 再次fork 降权到lightdm启动greeter 

