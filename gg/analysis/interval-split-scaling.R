library(tidyverse)

data <- read_csv("./data.csv")
benches <- c("2_6","4_2","3_5","4_5","5_5")
regex <- str_c(benches, collapse="|")
right_benches <- data %>% filter(grepl(regex, net))
no_outliers <- right_benches %>%
  filter(jobs < 32 | grepl("LAM", infra)) %>%
  # Outliers
  filter(trial != 0 | !grepl("2_6", net) | jobs !=512) %>%
  filter(trial != 0 | !grepl("3_5", net) | jobs !=1000) %>%
  filter(trial != 2 | !grepl("3_5", net) | jobs !=256) %>%
  filter(trial != 1 | !grepl("4_2", net) | jobs !=128) %>%
  filter(trial != 2 | !grepl("4_5", net) | jobs !=16) %>%
  filter(trial != 1 | !grepl("5_5", net) | jobs !=1000) %>%
  filter(trial != 1 | !grepl("5_5", net) | jobs !=512) %>%
  filter(hash != "76fe1c29" | !grepl("4_5", net) | jobs !=512)
no_outliers[no_outliers == 1024] <- 1000
series <- no_outliers %>%
  group_by(jobs,infra,divide_strategy,net) %>%
  summarise(rt_mean_trial = mean(runtime), rt_sd_trial = sd(runtime)) %>%
  group_by(jobs,infra,divide_strategy) %>%
  summarise(rt_mean_net = mean(rt_mean_trial))

#series[is.na(series)] <- 0
ggplot(series, aes(x = jobs, y = rt_mean_net, linetype = infra)) +
  geom_line() +
  geom_point() +
  scale_x_continuous(trans='log2') +
  scale_y_continuous(trans='log2', limits =c(32, 4096)) +
  labs(
    title = "Runtime improvement as parallelism grows",
    y = "Runtime (s)",
    x = "Number of Workers",
    linetype = "Infrastructure",
    color = "Split Strategy"
  )
ggsave("interval-split-scaling.png", width = 5, height = 4, units="in")
write_csv(series, "./interval-split-scaling.csv")
