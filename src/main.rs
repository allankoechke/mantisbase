use clap::Parser;
use mantisbase::cli::Cli;
use mantisbase::core::MantisBase;
use mantisbase::MantisBaseRunOutcome;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let cli = Cli::parse();
    let mut mantis = MantisBase::new();
    mantis.apply_cli(&cli)?;
    match mantis.run().await? {
        MantisBaseRunOutcome::NoSubcommand => std::process::exit(2),
        MantisBaseRunOutcome::RanAction => Ok(()),
    }
}
