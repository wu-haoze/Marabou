library(tidyverse)

data <- read_csv("./data.csv")
benches <- c("2_6","4_2","3_5","4_5","5_5","mnist")
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
series <- no_outliers %>%
  group_by(jobs,infra,net) %>%
  summarise(mean_rt = median(runtime), sd_rt = sd(runtime))
series[is.na(series)] <- 0
ggplot(series, aes(x = jobs, y = mean_rt, color = infra, linetype = net)) +
  geom_line() +
  geom_errorbar(aes(ymax = mean_rt + sd_rt, ymin = mean_rt - sd_rt)) +
  scale_x_continuous(trans='log2') +
  scale_y_continuous(trans='log2') +
  labs(
    title = "Runtime improvement as parallelism grows",
    y = "Runtime (s)",
    x = "Number of Workers",
    linetype = "Network (Proprty 1)",
    color = "Infrastructure"
  )
    
